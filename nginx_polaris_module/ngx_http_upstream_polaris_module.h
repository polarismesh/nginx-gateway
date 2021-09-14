#ifndef NGINX_MODULE_POLARIS_NGINX_POLARIS_MODULE_NGX_HTTP_UPSTREAM_POLARIS_MODULE_H_
#define NGINX_MODULE_POLARIS_NGINX_POLARIS_MODULE_NGX_HTTP_UPSTREAM_POLARIS_MODULE_H_

/*
 * Copyright (C) 2020-2025 Tencent Limited
 * author: jasonygyang@tencent.com
 */

extern "C" {
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
}

#include <time.h>
#include "polaris/consumer.h"

using std::string;
using std::vector;

// for polaris load-balance type
#define POLARIS_DEFAULT         0
#define POLARIS_WEIGHTED_RANDOM 1
#define POLARIS_RING_HASH       2
#define POLARIS_L5_CST_HASH     3

#define METADATA_ROUTE_FAILOVER_BY_NONE     0
#define METADATA_ROUTE_FAILOVER_BY_ALL      1
#define METADATA_ROUTE_FAILOVER_BY_NOT_KEY  2

/**
 * this come from ngx_http_upstream_dynamic_module.c
 */
typedef struct {
  ngx_int_t enabled;
  ngx_str_t polaris_service_namespace;
  ngx_array_t *polaris_service_namespace_lengths;
  ngx_array_t *polaris_service_namespace_values;

  ngx_str_t polaris_service_name;
  ngx_array_t *polaris_service_name_lengths;
  ngx_array_t *polaris_service_name_values;

  float polaris_timeout;

  ngx_str_t polaris_lb_key;
  ngx_int_t polaris_lb_mode;
  ngx_array_t *polaris_lb_key_lengths;
  ngx_array_t *polaris_lb_key_values;

  ngx_int_t polaris_dynamic_route_enabled;
  ngx_str_t polaris_dynamic_route_metadata_list;

  ngx_int_t metadata_route_failover_mode;
  ngx_int_t polaris_metadata_route_enabled;
  ngx_str_t polaris_metadata_route_metadata_list;

  ngx_str_t polaris_metadata_from_nginx_conf;             // 从Nginx.conf获取的metadata

  ngx_str_t polaris_fail_status_list;
  ngx_int_t polaris_fail_status_report_enabled;

  ngx_http_upstream_init_pt original_init_upstream;
  ngx_http_upstream_init_peer_pt original_init_peer;
} ngx_http_upstream_polaris_srv_conf_t;

/**
 * polaris context for every request
 */
typedef struct {
  ngx_pool_t *pool;
  ngx_log_t *log;

  // polaris param
  ngx_str_t polaris_service_namespace;
  ngx_str_t polaris_service_name;
  ngx_int_t polaris_timeout;
  ngx_str_t polaris_lb_key;
  polaris::LoadBalanceType polaris_lb_mode;

  ngx_int_t polaris_dynamic_route_enabled;
  ngx_str_t polaris_dynamic_route_metadata_list;

  polaris::MetadataFailoverType metadata_route_failover_mode;
  ngx_int_t polaris_metadata_route_enabled;
  ngx_str_t polaris_metadata_route_metadata_list;

  ngx_str_t polaris_fail_status_list;
  ngx_int_t polaris_fail_status_report_enabled;

  char name[64];

  // keep polaris get result
  char ip[NGX_INET_ADDRSTRLEN];
  int port;
  char instance_id[64];
  int polaris_ret;
  struct sockaddr_in addr;
  ngx_time_t polaris_start;
} ngx_http_upstream_polaris_ctx_t;

typedef struct {
  ngx_http_upstream_polaris_srv_conf_t *conf;
  ngx_http_upstream_t *upstream;
  void *data;
  ngx_http_request_t *request;
  ngx_event_get_peer_pt original_get_peer;
  ngx_event_free_peer_pt original_free_peer;
  ngx_http_upstream_polaris_ctx_t *ctx;
} ngx_http_upstream_polaris_peer_data_t;

extern "C" ngx_module_t ngx_http_upstream_polaris_module;

int polaris_init_params(ngx_http_upstream_polaris_srv_conf_t *srv, ngx_http_request_t *r,
                        ngx_http_upstream_polaris_ctx_t *ctx);

int polaris_get_addr(ngx_http_upstream_polaris_ctx_t *ctx);

int polaris_report(ngx_http_upstream_polaris_ctx_t *ctx);

void split_string(const string& s, vector<string>& v, const string& c);

#endif  // NGINX_MODULE_POLARIS_NGINX_POLARIS_MODULE_NGX_HTTP_UPSTREAM_POLARIS_MODULE_H_