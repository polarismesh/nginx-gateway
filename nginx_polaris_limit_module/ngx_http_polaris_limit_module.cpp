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
    ngx_str_t                           polaris_service_namespace;          // 服务命名空间
    ngx_array_t                        *polaris_service_namespace_lengths;
    ngx_array_t                        *polaris_service_namespace_values;

    ngx_str_t                           polaris_service_name;               // 服务名
    ngx_array_t                        *polaris_service_name_lengths;
    ngx_array_t                        *polaris_service_name_values;

    ngx_uint_t                          status_code;                        // 返回状态码
    ngx_uint_t                          timeout;                            // 拉取labelkey的超时时间
} ngx_http_polaris_limit_conf_t;

static ngx_conf_num_bounds_t  ngx_http_polaris_limit_status_bounds = {
    ngx_conf_check_num_bounds, 400, 599
};

static ngx_int_t ngx_http_polaris_limit_handler(ngx_http_request_t *r);
static char *ngx_http_polaris_limit(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_polaris_limit_init(ngx_conf_t *cf);
static void *ngx_http_polaris_limit_create_conf(ngx_conf_t *cf);
static char *ngx_http_polaris_limit_merge_conf(ngx_conf_t *cf, void *parent, void *child);
ngx_str_t get_polaris_service_namespace(ngx_http_polaris_limit_conf_t* loc, ngx_http_request_t* r);
ngx_str_t get_polaris_service_name(ngx_http_polaris_limit_conf_t* loc, ngx_http_request_t* r);
void split_str(const std::string& s, std::vector<std::string>& v, const std::string& c);
void get_labels_from_header(ngx_http_request_t* r, const std::set<std::string>*& label_keys,
                                      std::map<std::string, std::string>& keyword_map);

static ngx_command_t ngx_http_polaris_limit_commands[] = {
    { ngx_string("polaris_limit"),
      NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_polaris_limit,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },
    { ngx_string("polaris_limit_status"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_polaris_limit_conf_t, status_code),
      &ngx_http_polaris_limit_status_bounds },
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

    ngx_str_t                               polaris_service_namespace;
    ngx_str_t                               polaris_service_name;
    plcf = reinterpret_cast<ngx_http_polaris_limit_conf_t *>(
        ngx_http_get_module_loc_conf(r, ngx_http_polaris_limit_module));
    polaris_service_namespace = get_polaris_service_namespace(plcf, r);
    polaris_service_name = get_polaris_service_name(plcf, r);

    if (polaris_service_namespace.len == 0 || polaris_service_name.len == 0) {
        ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "polaris_limit invalid service");
        return NGX_DECLINED;                                    // 无效service，不限流
    }

    std::string service_namespace(reinterpret_cast<char *>(polaris_service_namespace.data),
                                                           polaris_service_namespace.len);
    std::string service_name(reinterpret_cast<char *>(polaris_service_name.data), polaris_service_name.len);
    polaris::ServiceKey serviceKey = {service_namespace, service_name};

    ret = Limit_API_SINGLETON.GetLimitApi()->FetchRuleLabelKeys(serviceKey, plcf->timeout, label_keys);

    ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "polaris_limit FetchRuleLabelKeys return is: %d", ret);
    if (ret == polaris::kReturnTimeout) {
        return NGX_DECLINED;                                    // 拉取labelkey超时，不限流
    } else if (ret != polaris::kReturnOk) {
        return plcf->status_code;                               // 返回为限流配置的状态码
    }

    get_labels_from_header(r, label_keys, labels);    // 从http header中获取labels

    quota_request.SetServiceNamespace(service_namespace);       // 设置限流规则对应服务的命名空间
    quota_request.SetServiceName(service_name);                 // 设置限流规则对应的服务名
    quota_request.SetLabels(labels);                            // 设置label用于匹配限流规则

    ret = Limit_API_SINGLETON.GetLimitApi()->GetQuota(quota_request, result);

    ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "polaris_limit GetQuota return is: %d", ret);
    if (ret == polaris::kReturnTimeout) {
        return NGX_DECLINED;                                    // GetQuota超时，不限流
    } else if (ret != polaris::kReturnOk) {
        return plcf->status_code;                               // 返回为限流配置的状态码
    }

    ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "polaris_limit result is: %d", result);
    if (result == polaris::kQuotaResultLimited) {
        return plcf->status_code;   // 请求被限制
    }
    return NGX_DECLINED;
}

/* 读取配置参数 polaris_limit */
static char *ngx_http_polaris_limit(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    ngx_http_polaris_limit_conf_t      *plcf;
    ngx_uint_t                          i;
    ngx_str_t                          *value;
    ngx_http_script_compile_t           sc;

    float                               time = 0;
    plcf = reinterpret_cast<ngx_http_polaris_limit_conf_t *>(conf);
    value = reinterpret_cast<ngx_str_t *>(cf->args->elts);

    for (i = 1; i < cf->args->nelts; i++) {
        if (ngx_strncmp(value[i].data, kServiceNamespace, kServiceNamespaceSize) == 0) {
            ngx_str_t s = {value[i].len - kServiceNamespaceSize, &value[i].data[kServiceNamespaceSize]};

            int n = ngx_http_script_variables_count(&s);

            if (n) {
              ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

              sc.cf = cf;
              sc.source = &s;
              sc.lengths = &plcf->polaris_service_namespace_lengths;
              sc.values = &plcf->polaris_service_namespace_values;
              sc.variables = n;
              sc.complete_lengths = 1;
              sc.complete_values = 1;

              if (ngx_http_script_compile(&sc) != NGX_OK) {
                ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "ngx_http_script_compile:%V, failed", value[i]);
                return const_cast<char *>("ngx_http_script_compile failed");
              }
            } else {
              plcf->polaris_service_namespace = s;
            }

            if (plcf->polaris_service_namespace.len <= 0 &&
                plcf->polaris_service_namespace_lengths == NULL) {
              plcf->polaris_service_namespace.data = NULL;
              plcf->polaris_service_namespace.len = 0;
              return const_cast<char *>("invalid polaris namespace");
            }
            continue;
        }

        if (ngx_strncmp(value[i].data, kServiceName, kServiceNameSize) == 0) {
            ngx_str_t s = {value[i].len - kServiceNameSize, &value[i].data[kServiceNameSize]};

            int n = ngx_http_script_variables_count(&s);

            if (n) {
              ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

              sc.cf = cf;
              sc.source = &s;
              sc.lengths = &plcf->polaris_service_name_lengths;
              sc.values = &plcf->polaris_service_name_values;
              sc.variables = n;
              sc.complete_lengths = 1;
              sc.complete_values = 1;

              if (ngx_http_script_compile(&sc) != NGX_OK) {
                ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "ngx_http_script_compile:%V, failed", value[i]);
                return const_cast<char *>("ngx_http_script_compile failed");
              }
            } else {
              plcf->polaris_service_name = s;
            }

            if (plcf->polaris_service_namespace.len <= 0 &&
                plcf->polaris_service_namespace_lengths == NULL) {
              plcf->polaris_service_namespace.data = NULL;
              plcf->polaris_service_namespace.len = 0;
              return const_cast<char *>("invalid polaris namespace");
            }
            continue;
        }

        if (ngx_strncmp(value[i].data, kTimeout, kTimeoutSize) == 0) {
            ngx_str_t s = {value[i].len - kTimeoutSize, &value[i].data[kTimeoutSize]};
            if (s.len > 0) {
                sscanf((const char *)s.data, "%f", &time);
            }

            if (s.len <= 0 || time <= 0) {
                ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "polaris_limit timeout:%lf invalid",
                                  time);
                return const_cast<char *>("invalid timeout key");
            }
            continue;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[i]);
        return static_cast<char *>(NGX_CONF_ERROR);
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

    conf->status_code = NGX_CONF_UNSET_UINT;        // 限流默认返回,服务不可用
    conf->timeout = 100;                            // 默认超时事件100ms
    return conf;
}

static char *
ngx_http_polaris_limit_merge_conf(ngx_conf_t *cf, void *parent, void *child) {
    ngx_http_polaris_limit_conf_t *prev = reinterpret_cast<ngx_http_polaris_limit_conf_t *>(parent);
    ngx_http_polaris_limit_conf_t *conf = reinterpret_cast<ngx_http_polaris_limit_conf_t *>(child);

    ngx_conf_merge_uint_value(conf->status_code, prev->status_code,
                              NGX_HTTP_SERVICE_UNAVAILABLE);
    return static_cast<char *>(NGX_CONF_OK);
}

void split_str(const std::string& s, std::vector<std::string>& v, const std::string& c) {
  std::string::size_type pos1, pos2;
  pos2 = s.find(c);
  pos1 = 0;
  while (std::string::npos != pos2) {
    v.push_back(s.substr(pos1, pos2-pos1));
    pos1 = pos2 + c.size();
    pos2 = s.find(c, pos1);
  }
  if (pos1 != s.length())
  v.push_back(s.substr(pos1));
}

void get_labels_from_header(ngx_http_request_t* r, const std::set<std::string>*& label_keys,
                                      std::map<std::string, std::string>& keyword_map) {
  ngx_http_headers_in_t headers_in = r->headers_in;
  ngx_list_t *headers = &(headers_in.headers);
  ngx_uint_t i;
  ngx_list_part_t *part = &(headers->part);
  ngx_table_elt_t *head;
  std::string head_key;
  std::string head_value;
  std::string cookie_value;

  for (part = &(headers->part); part != NULL; part = part->next) {
    head = reinterpret_cast<ngx_table_elt_t*>(part->elts);
    for (i = 0; i < part->nelts; ++i) {
      head_key = std::string(reinterpret_cast<char *>(head[i].key.data), head[i].key.len);
      if (head_key == "Cookie" || head_key == "cookie") {
        head_value = std::string(reinterpret_cast<char *>(head[i].value.data), head[i].value.len);
        cookie_value = head_value;
      }
      ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
        "deal with head_key: %s", head_key.c_str());

      if (label_keys->find(head_key) != label_keys->end()) {
        head_value = std::string(reinterpret_cast<char *>(head[i].value.data), head[i].value.len);
        ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
          "deal with head_key: %s, head_value: %s, is keyword.", head_key.c_str(),
          head_value.c_str());
        keyword_map[head_key] = head_value;
      }
    }
  }

  ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "get cookie: %s", cookie_value.c_str());
  std::string delimiter = "; ";
  std::vector<std::string> vec;
  split_str(cookie_value, vec, delimiter);
  for (size_t i = 0; i < vec.size(); ++i) {
    if (vec[i] != "") {
      std::vector<std::string> key_value;
      split_str(vec[i], key_value, "=");
      if (key_value.size() == 2) {
        if (label_keys->find(key_value[0]) != label_keys->end()) {
          ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
            "deal with head_key: %s, head_value: %s, is keyword.", key_value[0].c_str(),
            key_value[1].c_str());
          keyword_map[key_value[0]] = key_value[1];
        }
      }
    }
  }
}

ngx_str_t get_polaris_service_namespace(ngx_http_polaris_limit_conf_t* loc, ngx_http_request_t* r) {
  ngx_str_t polaris_service_namespace;
  if (loc->polaris_service_namespace_lengths != NULL) {
    if (ngx_http_script_run(r, &polaris_service_namespace,
                            loc->polaris_service_namespace_lengths->elts,
                            0, loc->polaris_service_namespace_values->elts) == NULL) {
      ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "polaris_init_params polaris_service_namespace failed!");
      return polaris_service_namespace;
    }
  } else {
    polaris_service_namespace = loc->polaris_service_namespace;
  }
  return polaris_service_namespace;
}

ngx_str_t get_polaris_service_name(ngx_http_polaris_limit_conf_t* loc, ngx_http_request_t* r) {
  ngx_str_t polaris_service_name;
  if (loc->polaris_service_name_lengths != NULL) {
    if (ngx_http_script_run(r, &polaris_service_name,
                            loc->polaris_service_name_lengths->elts,
                            0, loc->polaris_service_name_values->elts) == NULL) {
      ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "polaris_init_params polaris_service_name failed!");
      return polaris_service_name;
    }
  } else {
    polaris_service_name = loc->polaris_service_name;
  }
  return polaris_service_name;
}