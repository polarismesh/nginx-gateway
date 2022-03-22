/*
 * Copyright (C) 2020-2025 Tencent Limited
 * author: jasonygyang@tencent.com
 */

#include <sstream>
#include <algorithm>
#include "ngx_http_upstream_polaris_module.h"

using std::string;
using std::vector;

struct Comparator {
  bool operator() (const std::string& s1, const std::string& s2) const {
    std::string str1(s1.length(), ' ');
    std::string str2(s2.length(), ' ');
    std::transform(s1.begin(), s1.end(), str1.begin(), tolower);
    std::transform(s2.begin(), s2.end(), str2.begin(), tolower);
    return str1 < str2;
  }
};

class ConsumerApiWrapper {
 public:
  ConsumerApiWrapper() { 
    polaris::SetLogDir(POLARIS_LOG_DIR);
    m_consumer = polaris::ConsumerApi::CreateFromFile(POLARIS_CONFIG_DIR); 
  }

  static ConsumerApiWrapper& Instance() {
    static ConsumerApiWrapper consumer_api;
    return consumer_api;
  }

  polaris::ConsumerApi* GetConsumerApi() { return m_consumer; }

 private:
  polaris::ConsumerApi* m_consumer;
};

#define CONSUMER_API_SINGLETON ConsumerApiWrapper::Instance()

class MetadataRouteConsumerApiWrapper {
 public:
  MetadataRouteConsumerApiWrapper() {
    polaris::SetLogDir(POLARIS_LOG_DIR);
    m_consumer = polaris::ConsumerApi::CreateFromFile(POLARIS_CONFIG_DIR); 
  }

  static MetadataRouteConsumerApiWrapper& Instance() {
    static MetadataRouteConsumerApiWrapper consumer_api;
    return consumer_api;
  }

  polaris::ConsumerApi* GetConsumerApi() {
    return m_consumer;
  }

 private:
  polaris::ConsumerApi* m_consumer;
};

#define METADATA_ROUTE_CONSUMER_API_SINGLETON MetadataRouteConsumerApiWrapper::Instance()

void set_dynamic_route_source_info(ngx_http_upstream_polaris_ctx_t* ctx,
                                  polaris::GetOneInstanceRequest& request) {
  polaris::ServiceInfo service_info;
  std::string metadataString = std::string(
    reinterpret_cast<char *>(ctx->polaris_dynamic_route_metadata_list.data),
    ctx->polaris_dynamic_route_metadata_list.len);
  std::string delimiter = ";";
  vector<string> vec;
  split_string(metadataString, vec, delimiter);
  for (size_t i = 0; i < vec.size(); ++i) {
    if (vec[i] != "") {
      vector<string> key_value;
      split_string(vec[i], key_value, "=");
      if (key_value.size() == 2) {
        ngx_log_debug(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
          "metadata for source info %s = %s", key_value[0].c_str(), key_value[1].c_str());
        service_info.metadata_[key_value[0]] = key_value[1];
      }
    }
  }
  request.SetSourceService(service_info);
}

void set_metadata_route_meta_info(ngx_http_upstream_polaris_ctx_t* ctx,
                                  polaris::GetOneInstanceRequest& request) {
  std::map<std::string, std::string> metadata;
  std::string metadataString = std::string(
    reinterpret_cast<char *>(ctx->polaris_metadata_route_metadata_list.data),
    ctx->polaris_metadata_route_metadata_list.len);
  std::string delimiter = ";";
  vector<string> vec;
  split_string(metadataString, vec, delimiter);
  for (size_t i = 0; i < vec.size(); ++i) {
    if (vec[i] != "") {
      vector<string> key_value;
      split_string(vec[i], key_value, "=");
      if (key_value.size() == 2) {
        ngx_log_debug(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
          "metadata for metadata route %s = %s", key_value[0].c_str(), key_value[1].c_str());
        metadata[key_value[0]] = key_value[1];
      }
    }
  }

  request.SetMetadata(metadata);
}

void get_keyword_map_from_srv_conf(ngx_http_upstream_polaris_srv_conf_t* srv,
                                  std::map<string, string, Comparator>& keyword_map) {
  std::string str_metadata;
  if (srv->polaris_dynamic_route_enabled) {
    str_metadata = std::string(
    reinterpret_cast<char *>(srv->polaris_dynamic_route_metadata_list.data),
    srv->polaris_dynamic_route_metadata_list.len);
  } else {
    str_metadata = std::string(
    reinterpret_cast<char *>(srv->polaris_metadata_route_metadata_list.data),
    srv->polaris_metadata_route_metadata_list.len);
  }
  std::istringstream iss(str_metadata);
  std::string s;
  while (getline(iss, s, ';')) {
    if (s != "") {
      std::string delimiter = "=";
      vector<string> vec;
      split_string(s, vec, delimiter);
      if (vec.size() == 2 && vec[1] != "") {
        // keyword_map.insert(std::pair<std::string, std::string>(vec[0], vec[1]));
        keyword_map[vec[0]] = vec[1];
      } else {
        // keyword_map.insert(std::pair<std::string, std::string>(s, ""));
        keyword_map[vec[0]] = "";
      }
    }
  }
}

void get_context_metadata_from_header(ngx_http_request_t* r, ngx_http_upstream_polaris_ctx_t* ctx,
                                      std::map<std::string, std::string, Comparator>& keyword_map) {
  ngx_http_headers_in_t headers_in = r->headers_in;
  ngx_list_t *headers = &(headers_in.headers);
  ngx_uint_t i;
  ngx_list_part_t *part = &(headers->part);
  ngx_table_elt_t *head = reinterpret_cast<ngx_table_elt_t*>(part->elts);
  std::string head_key;
  std::string head_value;
  std::string cookie_value;
  for (i = 0; ; ++i) {
    if (i >= part->nelts) {
      if (part->next == NULL) {
        break;
      }
      part = part->next;
      head = reinterpret_cast<ngx_table_elt_t*>(part->elts);
      i = 0;
    }
    head_key = std::string(reinterpret_cast<char *>(head[i].key.data), head[i].key.len);
    head_value = std::string(reinterpret_cast<char *>(head[i].value.data), head[i].value.len);
    if (head_key == "Cookie" || head_key == "cookie") {
      cookie_value = head_value;
    }
    ngx_log_debug(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
      "deal with head_key: %s, head_value: %s", head_key.c_str(), head_value.c_str());
    if (keyword_map.find(head_key) != keyword_map.end()) {
      ngx_log_debug(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
        "deal with head_key: %s, head_value: %s, is keyword.", head_key.c_str(),
        head_value.c_str());
      keyword_map[head_key] = head_value;
    }
  }

  ngx_log_debug(NGX_LOG_DEBUG_HTTP, ctx->log, 0, "get cookie: %s", cookie_value.c_str());
  std::string delimiter = "; ";
  vector<string> vec;
  split_string(cookie_value, vec, delimiter);
  for (size_t i = 0; i < vec.size(); ++i) {
    if (vec[i] != "") {
      vector<string> key_value;
      split_string(vec[i], key_value, "=");
      if (key_value.size() == 2) {
        if (keyword_map.find(key_value[0]) != keyword_map.end()) {
          ngx_log_debug(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
            "deal with head_key: %s, head_value: %s, is keyword.", key_value[0].c_str(),
            key_value[1].c_str());
          keyword_map[key_value[0]] = key_value[1];
        }
      }
    }
  }
}

void get_context_metadata_from_url(ngx_http_request_t* r, ngx_http_upstream_polaris_ctx_t* ctx,
                                      std::map<std::string, std::string, Comparator>& keyword_map) {
  ngx_str_t args_ngx_str = r->args;
  if (args_ngx_str.len > 0) {
    std::string args_str = string(reinterpret_cast<char *>(args_ngx_str.data), args_ngx_str.len);
    vector<string> vec;
    split_string(args_str, vec, "&");

    for (uint32_t i = 0; i < vec.size(); ++i) {
      if (vec[i].length() > 0) {
        vector<string> args_vec;
        split_string(vec[i], args_vec, "=");
        if (args_vec.size() == 2 && keyword_map.find(args_vec[0]) != keyword_map.end()) {
          ngx_log_debug(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
            "deal with args_key: %s, args_value: %s, is keyword.", args_vec[0].c_str(),
            args_vec[1].c_str());
          keyword_map[args_vec[0]] = args_vec[1];
        }
      }
    }
  }
}

void generate_metadata_str(ngx_http_upstream_polaris_ctx_t* ctx, std::string& ctx_metadata_list,
                          std::map<string, string, Comparator>& keyword_map) {
  std::map<std::string, std::string>::iterator iter;
  for (iter = keyword_map.begin(); iter != keyword_map.end(); ++iter) {
    if (iter->second != "") {
      ngx_log_debug(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
        "metadata result after read request, key: %s, value: %s", iter->first.c_str(), iter->second.c_str());
      ctx_metadata_list = ctx_metadata_list + iter->first + "=" + iter->second + ";";
    }
  }
}

void set_context_metadata_str(ngx_http_upstream_polaris_ctx_t* ctx, string& ctx_metadata_list) {
  if (ctx->polaris_dynamic_route_enabled) {
    ctx->polaris_dynamic_route_metadata_list.len = ctx_metadata_list.length();
    ctx->polaris_dynamic_route_metadata_list.data = reinterpret_cast<u_char*>(ngx_palloc(
      ctx->pool, ctx->polaris_dynamic_route_metadata_list.len));
    memcpy(ctx->polaris_dynamic_route_metadata_list.data, &ctx_metadata_list[0],
      ctx->polaris_dynamic_route_metadata_list.len);
  } else {
    ctx->polaris_metadata_route_metadata_list.len = ctx_metadata_list.length();
    ctx->polaris_metadata_route_metadata_list.data = reinterpret_cast<u_char*>(ngx_palloc(
      ctx->pool, ctx->polaris_metadata_route_metadata_list.len));
    memcpy(ctx->polaris_metadata_route_metadata_list.data, &ctx_metadata_list[0],
      ctx->polaris_metadata_route_metadata_list.len);
  }
}

void set_context_metadata(ngx_http_upstream_polaris_srv_conf_t* srv, ngx_http_request_t* r,
                              ngx_http_upstream_polaris_ctx_t* ctx) {
  ngx_log_debug(NGX_LOG_DEBUG_HTTP, ctx->log, 0, "metadata list from wrapper: %V",
    &srv->polaris_dynamic_route_metadata_list);
  std::map<std::string, std::string, Comparator> keyword_map;
  get_keyword_map_from_srv_conf(srv, keyword_map);              // 将从file或rainbow获取的metadata写map中
  if (keyword_map.size() == 0) {
    ngx_log_debug(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
    "keyword_map is empty, don't set context_metadata.", &ctx->polaris_dynamic_route_metadata_list);
  }

  get_context_metadata_from_header(r, ctx, keyword_map);        // 从header和url中读metadata到map中
  get_context_metadata_from_url(r, ctx, keyword_map);

  std::string ctx_metadata_list;
  generate_metadata_str(ctx, ctx_metadata_list, keyword_map);   // 将map中的metadata写ctx_metadata_list
  ngx_log_debug(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
    "metadata after read request, list: %s", ctx_metadata_list.c_str());

  std::string metadata_from_nginx(reinterpret_cast<char *>(srv->polaris_metadata_from_nginx_conf.data),
    srv->polaris_metadata_from_nginx_conf.len);
  ctx_metadata_list += metadata_from_nginx;                     // 将从Nginx.conf配置的数据追加到ctx_metadata_list末尾

  set_context_metadata_str(ctx, ctx_metadata_list);             // 将ctx_metadata_list放ctx，用于获取服务
  ngx_log_debug(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
    "metadata key list from ctx: %V", &ctx->polaris_dynamic_route_metadata_list);
}

void set_ctx_pool(ngx_http_upstream_polaris_ctx_t* ctx, ngx_http_request_t* r) {
  if (ctx->pool == NULL) {
    ctx->pool = r->pool;
  }
}

void set_ctx_log(ngx_http_upstream_polaris_ctx_t* ctx, ngx_http_request_t* r) {
  if (ctx->log == NULL) {
    ctx->log = r->connection->log;
  }
}
int set_polaris_lb_key(ngx_http_upstream_polaris_srv_conf_t* srv, ngx_http_request_t* r,
                        ngx_http_upstream_polaris_ctx_t* ctx) {
  ngx_str_t polaris_lb_key;
  if (srv->polaris_lb_key_lengths != NULL) {
    if (ngx_http_script_run(r, &polaris_lb_key, srv->polaris_lb_key_lengths->elts,
                            0, srv->polaris_lb_key_values->elts) == NULL) {
      ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "polaris_init_params polaris_lb_key failed!");
      return NGX_ERROR;
    }
  } else {
    polaris_lb_key = srv->polaris_lb_key;
  }
  ctx->polaris_lb_key = polaris_lb_key;
  return NGX_OK;
}

int set_polaris_fail_report_status_list(ngx_http_upstream_polaris_srv_conf_t* srv, ngx_http_request_t* r,
                        ngx_http_upstream_polaris_ctx_t* ctx) {
  ngx_log_debug(NGX_LOG_DEBUG_HTTP, ctx->log, 0, "fail status list from wrapper: %V",
    &srv->polaris_fail_status_list);
  ctx->polaris_fail_status_list = srv->polaris_fail_status_list;
  if (srv->polaris_fail_status_list.len > 0) {
    ctx->polaris_fail_status_report_enabled = true;
  } else {
    ctx->polaris_fail_status_report_enabled = false;
  }
  return NGX_OK;
}

int set_polaris_service_namespace(ngx_http_upstream_polaris_srv_conf_t* srv, ngx_http_request_t* r,
                        ngx_http_upstream_polaris_ctx_t* ctx) {
  ngx_str_t polaris_service_namespace;
  if (srv->polaris_service_namespace_lengths != NULL) {
    if (ngx_http_script_run(r, &polaris_service_namespace,
                            srv->polaris_service_namespace_lengths->elts,
                            0, srv->polaris_service_namespace_values->elts) == NULL) {
      ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "polaris_init_params polaris_service_namespace failed!");
      return NGX_ERROR;
    }
  } else {
    polaris_service_namespace = srv->polaris_service_namespace;
  }
  ctx->polaris_service_namespace = polaris_service_namespace;
  return NGX_OK;
}

int set_polaris_service_name(ngx_http_upstream_polaris_srv_conf_t* srv, ngx_http_request_t* r,
                        ngx_http_upstream_polaris_ctx_t* ctx) {
  ngx_str_t polaris_service_name;
  if (srv->polaris_service_name_lengths != NULL) {
    if (ngx_http_script_run(r, &polaris_service_name, srv->polaris_service_name_lengths->elts,
                            0, srv->polaris_service_name_values->elts) == NULL) {
      ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "polaris_init_params polaris_service_name failed!");
      return NGX_ERROR;
    }
  } else {
    polaris_service_name = srv->polaris_service_name;
  }
  ctx->polaris_service_name = polaris_service_name;
  return NGX_OK;
}

void set_polaris_lb_mode(ngx_http_upstream_polaris_srv_conf_t* srv, ngx_http_request_t* r,
                        ngx_http_upstream_polaris_ctx_t* ctx) {
  switch (srv->polaris_lb_mode) {
    case POLARIS_DEFAULT:
      ctx->polaris_lb_mode = 0;
      break;
    case POLARIS_WEIGHTED_RANDOM:
      ctx->polaris_lb_mode = 0;
      break;
    case POLARIS_RING_HASH:
      ctx->polaris_lb_mode = 1;
      break;
    case POLARIS_L5_CST_HASH:
      ctx->polaris_lb_mode = 1;
      break;
    default:
      ctx->polaris_lb_mode = 0;
  }
}

void set_metadata_route_failover_mode(ngx_http_upstream_polaris_srv_conf_t* srv,
  ngx_http_request_t* r, ngx_http_upstream_polaris_ctx_t* ctx) {
  switch (srv->metadata_route_failover_mode) {
    case METADATA_ROUTE_FAILOVER_BY_NONE:
      ctx->metadata_route_failover_mode = polaris::kMetadataFailoverNone;
      break;
    case METADATA_ROUTE_FAILOVER_BY_ALL:
      ctx->metadata_route_failover_mode = polaris::kMetadataFailoverAll;
      break;
    case METADATA_ROUTE_FAILOVER_BY_NOT_KEY:
      ctx->metadata_route_failover_mode = polaris::kMetadataFailoverNotKey;
      break;
    default:
      ctx->metadata_route_failover_mode = polaris::kMetadataFailoverNone;
  }
}

int polaris_init_params(ngx_http_upstream_polaris_srv_conf_t* srv, ngx_http_request_t* r,
                        ngx_http_upstream_polaris_ctx_t* ctx) {
  set_ctx_pool(ctx, r);
  set_ctx_log(ctx, r);

  int ret = set_polaris_service_namespace(srv, r, ctx);
  if (ret) return ret;

  ret = set_polaris_service_name(srv, r, ctx);
  if (ret) return ret;

  ctx->polaris_timeout = static_cast<int>(srv->polaris_timeout * 1000);

  ctx->polaris_dynamic_route_enabled = srv->polaris_dynamic_route_enabled;

  ctx->polaris_metadata_route_enabled = srv->polaris_metadata_route_enabled;

  ret = set_polaris_lb_key(srv, r, ctx);
  if (ret) return ret;

  ret = set_polaris_fail_report_status_list(srv, r, ctx);
  if (ret) return ret;

  set_polaris_lb_mode(srv, r, ctx);

  set_metadata_route_failover_mode(srv, r, ctx);

  snprintf(ctx->name, sizeof(ctx->name), "%s#%s#%s", ctx->polaris_service_namespace.data,
           ctx->polaris_service_name.data, ctx->polaris_lb_key.data);

  if (ctx->polaris_dynamic_route_enabled || ctx->polaris_metadata_route_enabled) {
    set_context_metadata(srv, r, ctx);
  }

  ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "url args: %V", &r->args);

  return NGX_OK;
}

void set_request_hash_str(ngx_http_upstream_polaris_ctx_t* ctx,
                          polaris::GetOneInstanceRequest& request) {
  if (ctx->polaris_lb_key.len > 0) {
    std::string hashKey(reinterpret_cast<char*>(ctx->polaris_lb_key.data), ctx->polaris_lb_key.len);
    request.SetHashString(hashKey);
    ngx_log_debug(NGX_LOG_DEBUG_HTTP, ctx->log, 0, "hash key: %s", hashKey.c_str());
  }
}

int polaris_get_addr(ngx_http_upstream_polaris_ctx_t* ctx) {
  ngx_log_debug(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
    "polaris dynamic route metadata list from ctx: %V", &ctx->polaris_dynamic_route_metadata_list);
    ngx_log_debug(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
    "polaris metadata route metadata list from ctx: %V", &ctx->polaris_metadata_route_metadata_list);
  memcpy(&ctx->polaris_start, ngx_timeofday(), sizeof(ngx_time_t));

  std::string serviceNameSpace(reinterpret_cast<char*>(ctx->polaris_service_namespace.data),
    ctx->polaris_service_namespace.len);
  std::string serviceName(reinterpret_cast<char*>(ctx->polaris_service_name.data), ctx->polaris_service_name.len);
  polaris::ServiceKey serviceKey = {serviceNameSpace, serviceName};
  polaris::Instance instance;
  polaris::GetOneInstanceRequest request(serviceKey);
  request.SetTimeout(ctx->polaris_timeout);
  if (ctx->polaris_lb_mode > 0) {
    request.SetLoadBalanceType(polaris::kLoadBalanceTypeRingHash);
  }
  set_request_hash_str(ctx, request);

  if (ctx->polaris_dynamic_route_enabled) {
    set_dynamic_route_source_info(ctx, request);
  }

  if (ctx->polaris_metadata_route_enabled) {
    set_metadata_route_meta_info(ctx, request);
    request.SetMetadataFailover(ctx->metadata_route_failover_mode);
  }

  polaris::ReturnCode ret;
  if (ctx->polaris_metadata_route_enabled) {
    ret = METADATA_ROUTE_CONSUMER_API_SINGLETON.GetConsumerApi()->GetOneInstance(request, instance);
  } else {
    ret = CONSUMER_API_SINGLETON.GetConsumerApi()->GetOneInstance(request, instance);
  }

  ctx->polaris_ret = ret;

  memset(&ctx->addr, 0, sizeof(struct sockaddr_in));

  if (ret == polaris::kReturnOk) {
    ngx_log_debug(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
                  "polaris get instance success, namespace: %s, name: %s, host: %s, port: %d, "
                  "instance id: %s, timeout: %d",
                  serviceNameSpace.c_str(), serviceName.c_str(), instance.GetHost().c_str(),
                  instance.GetPort(), instance.GetId().c_str(), ctx->polaris_timeout);
    strncpy(ctx->ip, instance.GetHost().c_str(), instance.GetHost().size() + 1);

    ctx->port                 = instance.GetPort();
    snprintf(ctx->instance_id, sizeof(ctx->instance_id), "%s", instance.GetId().c_str());
    ctx->addr.sin_family      = AF_INET;
    ctx->addr.sin_port        = htons(ctx->port);
    ctx->addr.sin_addr.s_addr = inet_addr(ctx->ip);
    snprintf(ctx->name, sizeof(ctx->name), "%s:%d", ctx->ip, ctx->port);
  } else {
    ngx_log_error(NGX_LOG_ERR, ctx->log, 0,
                  "polaris get instance fail, namespace: %s, name: %s, ret: %d",
                  serviceNameSpace.c_str(), serviceName.c_str(), ret);
  }

  return ret;
}

int polaris_report(ngx_http_upstream_polaris_ctx_t* ctx) {
  std::string serviceNameSpace(reinterpret_cast<char*>(ctx->polaris_service_namespace.data),
    ctx->polaris_service_namespace.len);
  std::string serviceName(reinterpret_cast<char*>(ctx->polaris_service_name.data), ctx->polaris_service_name.len);
  std::string instanceId(ctx->instance_id);
  float time_out = (ngx_cached_time->sec - ctx->polaris_start.sec) * 1000 +
                   (ngx_cached_time->msec - ctx->polaris_start.msec);

  polaris::ServiceCallResult result;
  result.SetServiceNamespace(serviceNameSpace);
  result.SetServiceName(serviceName);
  result.SetInstanceId(instanceId);
  result.SetDelay(time_out);
  if (ctx->polaris_ret == 0) {
    result.SetRetCode(0);
    result.SetRetStatus(polaris::kCallRetOk);
  } else {
    result.SetRetCode(ctx->polaris_ret);
    result.SetRetStatus(polaris::kCallRetError);
  }

  polaris::ReturnCode ret_code =
      CONSUMER_API_SINGLETON.GetConsumerApi()->UpdateServiceCallResult(result);
  if (ret_code != polaris::kReturnOk) {
    ngx_log_error(NGX_LOG_ERR, ctx->log, 0,
                  "update call result for instance with error:%s, instance id: %s",
                  polaris::ReturnCodeToMsg(ret_code).c_str(), instanceId.c_str());
  } else {
    ngx_log_debug(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
                  "updata one instance success, namespace: %s, name: %s, instance id: %s",
                  serviceNameSpace.c_str(), serviceName.c_str(), instanceId.c_str());
  }
  return 0;
}