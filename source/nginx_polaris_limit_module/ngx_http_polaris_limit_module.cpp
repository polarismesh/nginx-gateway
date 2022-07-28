//  Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
//
//  Licensed under the BSD 3-Clause License (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//  https://opensource.org/licenses/BSD-3-Clause
//
//  Unless required by applicable law or agreed to in writing, software distributed
//  under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
//  CONDITIONS OF ANY KIND, either express or implied. See the License for the specific
//  language governing permissions and limitations under the License.
//

#include "ngx_http_polaris_limit_module.h"

typedef struct {
    ngx_str_t                           service_namespace;                  // 命名空间

    ngx_str_t                           service_name;                       // 服务名

    ngx_int_t                           status_code;                        // 返回码

} ngx_http_polaris_limit_conf_t;

static ngx_int_t ngx_http_polaris_limit_handler(ngx_http_request_t *r);
static char *ngx_http_polaris_limit_conf_set(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_polaris_limit_init(ngx_conf_t *cf);
static void *ngx_http_polaris_limit_create_conf(ngx_conf_t *cf);
static char *ngx_http_polaris_limit_merge_conf(ngx_conf_t *cf, void *parent, void *child);
static void split_query(std::string& s, std::map<std::string, std::string>& v);
static void parse_query_token(const std::string& token, std::map<std::string, std::string>& v);
static void get_labels_from_request(ngx_http_request_t* r, const std::set<std::string>*& label_keys,
                                      std::map<std::string, std::string>& keyword_map);
static void join_set_str(const ngx_log_t *log, const std::set<std::string>*& label_keys, std::string& labels_str);
static void join_map_str(const ngx_log_t *log, const std::map<std::string, std::string>& labels, std::string& labels_str);

static ngx_command_t ngx_http_polaris_limit_commands[] = {
    { ngx_string("polaris_rate_limiting"),
      NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_polaris_limit_conf_set,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },
    ngx_null_command
};

static ngx_http_module_t ngx_http_polaris_limit_module_ctx = {
    NULL,                                       /* preconfiguration */
    ngx_http_polaris_limit_init,                /* postconfiguration */

    NULL,                                       /* create main configuration */
    NULL,                                       /* init main configuration */

    NULL,                                       /* create server configuration */
    NULL,                                       /* merge server configuration */

    ngx_http_polaris_limit_create_conf,         /* create location configuration */
    ngx_http_polaris_limit_merge_conf,          /* merge location configuration */
};

ngx_module_t ngx_http_polaris_limit_module = {
    NGX_MODULE_V1,
    &ngx_http_polaris_limit_module_ctx,         /* module context */
    ngx_http_polaris_limit_commands,            /* module directives */
    NGX_HTTP_MODULE,                            /* module type */
    NULL,                                       /* init master */
    NULL,                                       /* init module */
    NULL,                                       /* init process */
    NULL,                                       /* init thread */
    NULL,                                       /* exit thread */
    NULL,                                       /* exit process */
    NULL,                                       /* exit master */
    NGX_MODULE_V1_PADDING
};

/* 处理函数 */
static ngx_int_t ngx_http_polaris_limit_handler(ngx_http_request_t *r) {
    ngx_http_polaris_limit_conf_t          *plcf;
    polaris::QuotaRequest                   quota_request;
    polaris::ReturnCode                     ret;
    polaris::QuotaResultCode                result;
    std::map<std::string, std::string>      labels;
    const std::set<std::string>            *label_keys;

    ngx_str_t                               service_namespace_str;
    ngx_str_t                               service_name_str;
    plcf = reinterpret_cast<ngx_http_polaris_limit_conf_t *>(
        ngx_http_get_module_loc_conf(r, ngx_http_polaris_limit_module));
    service_namespace_str = plcf->service_namespace;
    service_name_str =plcf->service_name;

    std::string service_namespace(reinterpret_cast<char *>(service_namespace_str.data), service_namespace_str.len);
    std::string service_name(reinterpret_cast<char *>(service_name_str.data), service_name_str.len);
    polaris::ServiceKey serviceKey = {service_namespace, service_name};
    std::string method = std::string(reinterpret_cast<char *>(r->uri.data), r->uri.len);
    ret = Limit_API_SINGLETON.GetLimitApi()->FetchRuleLabelKeys(serviceKey, label_keys);

    std::string labels_key_str;
    if (label_keys != NULL) {
      join_set_str(r->connection->log, label_keys, labels_key_str);
    }
    ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[PolarisRateLimiting] FetchRuleLabelKeys return is: %d, labels %s", ret, labels_key_str.c_str());
    if (ret == polaris::kReturnTimeout) {
        return NGX_DECLINED;                                    // 拉取labelkey超时，不限流
    } else if (ret != polaris::kReturnOk) {
        return plcf->status_code;                               // 返回为限流配置的状态码
    }

    get_labels_from_request(r, label_keys, labels);    // 从http 请求中获取labels

    quota_request.SetServiceNamespace(service_namespace);       // 设置限流规则对应服务的命名空间
    quota_request.SetServiceName(service_name);                 // 设置限流规则对应的服务名
    quota_request.SetLabels(labels);                            // 设置label用于匹配限流规则

    std::string labels_values_str;
    join_map_str(r->connection->log, labels, labels_values_str);
    ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, 
        "[PolarisRateLimiting] quota_request namespace %s, service %s, labels %s", service_namespace.c_str(), service_name.c_str(), labels_values_str.c_str());
    
    ret = Limit_API_SINGLETON.GetLimitApi()->GetQuota(quota_request, result);

    ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[PolarisRateLimiting] GetQuota return is: %d", ret);
    if (ret == polaris::kReturnTimeout) {
        return NGX_DECLINED;                                    // GetQuota超时，不限流
    } else if (ret != polaris::kReturnOk) {
        return plcf->status_code;                               // 返回为限流配置的状态码
    }
  
    ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[PolarisRateLimiting] result is: %d", result);
    if (result == polaris::kQuotaResultLimited) {
        return plcf->status_code;   // 请求被限制
    }
    return NGX_DECLINED;
}

static void join_set_str(const ngx_log_t *log, const std::set<std::string>*& label_keys, std::string& labels_str) {
  if (log->log_level < NGX_LOG_DEBUG) {
    return;
  }
  for (std::set<std::string>::iterator it = label_keys->begin(); it != label_keys->end(); it++) {
    labels_str += *it;
    labels_str += " ";
  }
}

static void join_map_str(const ngx_log_t *log, const std::map<std::string, std::string>& labels, std::string& labels_str) {
  if (log->log_level < NGX_LOG_DEBUG) {
    return;
  }
  for (std::map<std::string, std::string>::const_iterator it = labels.begin(); it != labels.end(); it++) {
    labels_str += it->first;
    labels_str += "=";
    labels_str += it->second;
    labels_str += " ";
  }
}

/* 读取配置参数 polaris_limit */
static char *ngx_http_polaris_limit_conf_set(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    ngx_http_polaris_limit_conf_t      *plcf;
    ngx_uint_t                          i;
    ngx_str_t                          *value;
    plcf = reinterpret_cast<ngx_http_polaris_limit_conf_t *>(conf);
    value = reinterpret_cast<ngx_str_t *>(cf->args->elts);

    for (i = 1; i < cf->args->nelts; i++) {
        if (ngx_strncmp(value[i].data, KEY_NAMESPACE, KEY_NAMESPACE_SIZE) == 0) {
            size_t ns_size = value[i].len - KEY_NAMESPACE_SIZE;
            if (ns_size <= 0) {
                ngx_str_t namespace_str = {DEFAULT_NAMESPACE_SIZE, (u_char *)DEFAULT_NAMESPACE};
                plcf->service_namespace = namespace_str;
                ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0, "[PolarisRateLimiting] nginx namespace not set, use 'default' as namespace");
            } else {
                ngx_str_t namespace_str = {value[i].len - KEY_NAMESPACE_SIZE, &value[i].data[KEY_NAMESPACE_SIZE]};
                plcf->service_namespace = namespace_str;
                ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0, "[PolarisRateLimiting] use %v as nginx namespace", namespace_str);
            }
            continue;
        }

        if (ngx_strncmp(value[i].data, KEY_SERVICE_NAME, KEY_SERVICE_NAME_SIZE) == 0) {
            ngx_str_t svc_name_str = {value[i].len - KEY_SERVICE_NAME_SIZE, &value[i].data[KEY_SERVICE_NAME_SIZE]};
            plcf->service_name = svc_name_str;
            if (plcf->service_name.len <= 0) {
                plcf->service_name.data = NULL;
                plcf->service_name.len = 0;
                ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "[PolarisRateLimiting] service name not set");
                return static_cast<char *>(NGX_CONF_ERROR);
            } else {
                ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0, "[PolarisRateLimiting] use %v as service", svc_name_str);
            }
            continue;
        }
    }

    return static_cast<char *>(NGX_CONF_OK);
}

/* 初始化limit模块 */
static ngx_int_t ngx_http_polaris_limit_init(ngx_conf_t *cf) {
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    // http核心模块的main_conf
    cmcf = reinterpret_cast<ngx_http_core_main_conf_t *>(
        ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module));

    // 准备挂载模块，获取迭代器
    h = reinterpret_cast<ngx_http_handler_pt *>(
        ngx_array_push(&cmcf->phases[NGX_HTTP_PREACCESS_PHASE].handlers));
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_polaris_limit_handler;    // 挂载limit_req处理函数
    return NGX_OK;
}

/* 创建 conf */
static void *ngx_http_polaris_limit_create_conf(ngx_conf_t *cf) {
    ngx_http_polaris_limit_conf_t       *conf;

    conf = reinterpret_cast<ngx_http_polaris_limit_conf_t *>(
        ngx_pcalloc(cf->pool, sizeof(ngx_http_polaris_limit_conf_t)));
    if (conf == NULL) {
        return NULL;
    }

    conf->status_code = 429;        // 限流默认返回429
    return conf;
}

static char * ngx_http_polaris_limit_merge_conf(ngx_conf_t *cf, void *parent, void *child) {
    //ngx_http_polaris_limit_conf_t *prev = reinterpret_cast<ngx_http_polaris_limit_conf_t *>(parent);
    //ngx_http_polaris_limit_conf_t *conf = reinterpret_cast<ngx_http_polaris_limit_conf_t *>(child);

    //ngx_conf_merge_uint_value(conf->status_code, prev->status_code, 429);
    return static_cast<char *>(NGX_CONF_OK);
}

static const std::string delimiter = ">=";

static void split_query(std::string& s, std::map<std::string, std::string>& v) {
  size_t pos = 0;
  std::string token;
  while ((pos = s.find(delimiter)) != std::string::npos) {
    token = s.substr(0, pos);
    parse_query_token(token, v);
    s.erase(0, pos + delimiter.length());
  }
  parse_query_token(token, v);
}

static void parse_query_token(const std::string& token, std::map<std::string, std::string>& v) {
  std::size_t found = token.find("=");
  if (found != std::string::npos) {
    std::string key = token.substr(0, found);
    std::string value = token.substr(found + 1);
    v[key] = value;
  }
}

static void get_labels_from_request(ngx_http_request_t* r, const std::set<std::string>*& label_keys,
                                      std::map<std::string, std::string>& keyword_map) {
  if (label_keys == NULL) {
    return;
  }                                      
  if (label_keys->find(LABEL_KEY_METHOD) != label_keys->end()) {
    std::string method_name(reinterpret_cast<char *>(r->method_name.data), r->method_name.len);
    keyword_map[LABEL_KEY_METHOD] = method_name;
  }

  std::set<std::string> rule_query_keys ;
  std::set<std::string> rule_header_keys;
  for (std::set<std::string>::iterator it = label_keys->begin(); it != label_keys->end(); it++) {
    std::string label_key = *it;
    if (label_key.rfind(LABEL_KEY_HEADER, 0) == 0) {
        std::string rule_header_key = label_key.substr(LABEL_KEY_HEADER.length());
        if (!rule_header_key.empty()) {
          rule_header_keys.insert(rule_header_key);
        }
        continue;
    }
    if (label_key.rfind(LABEL_KEY_QUERY, 0) == 0) {
        std::string rule_query_key = label_key.substr(LABEL_KEY_QUERY.length());
        if (!rule_query_key.empty()) {
          rule_query_keys.insert(rule_query_key);
        }
        continue;
    }
  }

  ngx_http_headers_in_t headers_in = r->headers_in;
  ngx_list_t *headers = &(headers_in.headers);
  ngx_uint_t i;
  ngx_list_part_t *part = &(headers->part);
  ngx_table_elt_t *head;
  std::string head_key;
  std::string head_value;
  std::string query_str;
  // parse header
  if (!rule_header_keys.empty()) {
    for (part = &(headers->part); part != NULL; part = part->next) {
      head = reinterpret_cast<ngx_table_elt_t*>(part->elts);
      for (i = 0; i < part->nelts; ++i) {
        head_key = std::string(reinterpret_cast<char *>(head[i].key.data), head[i].key.len);
        ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
          "deal with head_key: %s", head_key.c_str());

        if (rule_header_keys.find(head_key) != rule_header_keys.end()) {
          head_value = std::string(reinterpret_cast<char *>(head[i].value.data), head[i].value.len);
          ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
            "deal with head_key: %s, head_value: %s, is keyword.", head_key.c_str(),
            head_value.c_str());
          keyword_map[LABEL_KEY_HEADER + head_key] = head_value;
        }
      }
    }
  }
  // parse query
  std::map<std::string, std::string> query_values;
  if (!rule_query_keys.empty()) {
    query_str = std::string(reinterpret_cast<char *>(r->args.data), r->args.len);
    if (!query_str.empty()) {
      split_query(query_str, query_values);
      std::map<std::string, std::string>::iterator found;
      for (std::set<std::string>::iterator it = rule_query_keys.begin(); it != rule_query_keys.end(); it++) {
        std::string rule_query_key = *it;
        found = query_values.find(rule_query_key);
        if (found != query_values.end()) {
          keyword_map[LABEL_KEY_QUERY + rule_query_key] = found->second;
        }
      }
    }
  }
}