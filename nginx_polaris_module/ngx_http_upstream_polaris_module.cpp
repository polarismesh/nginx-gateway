/*
 * Copyright (C) 2020-2025 Tencent Limited
 * author: jasonygyang@tencent.com
 */

#include <fstream>
#include <string>
#include <sstream>
#include "ngx_http_upstream_polaris_module.h"

#define ROUTE_TYPE_DYNAMIC         0
#define ROUTE_TYPE_METADATA        1

const char* polaris_module_version = "nginx_polaris_v0.4.0";
const char* polaris_metadata_root_dir = "/polaris/dynamic_route/";
const char* polaris_metadata_route_meta_root_dir = "/polaris/metadata_route/";
const char* polaris_fail_status_root_dir = "/polaris/fail_report/";

static char *ngx_http_upstream_polaris_set_handler(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static ngx_command_t ngx_http_upstream_polaris_commands[] = {
    {ngx_string("polaris"), NGX_HTTP_UPS_CONF | NGX_CONF_1MORE,
     ngx_http_upstream_polaris_set_handler, 0, 0, NULL},
    ngx_null_command};

static void *ngx_http_upstream_polaris_create_conf(ngx_conf_t *cf);

static ngx_http_module_t ngx_http_upstream_polaris_module_ctx = {
    NULL, /* preconfiguration */
    NULL, /* postconfiguration */

    NULL, /* create main configuration */
    NULL, /* init main configuration */

    ngx_http_upstream_polaris_create_conf, /* create server configuration */
    NULL,                                  /* merge server configuration */

    NULL, /* create location configuration */
    NULL  /* merge location configuration */
};

extern "C" {
ngx_module_t ngx_http_upstream_polaris_module = {
    NGX_MODULE_V1,
    &ngx_http_upstream_polaris_module_ctx, /* module context */
    ngx_http_upstream_polaris_commands,    /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING};
}

void split_string(const string& s, vector<string>& v, const string& c) {
  string::size_type pos1, pos2;
  pos2 = s.find(c);
  pos1 = 0;
  while (string::npos != pos2) {
    v.push_back(s.substr(pos1, pos2-pos1));
    pos1 = pos2 + c.size();
    pos2 = s.find(c, pos1);
  }
  if (pos1 != s.length())
  v.push_back(s.substr(pos1));
}

void read_name_metadata_from_file(ngx_conf_t *cf,
  ngx_http_upstream_polaris_srv_conf_t *dcf, std::string& name_key_list) {
  std::string namespace_str(
  reinterpret_cast<char *>(dcf->polaris_service_namespace.data),
    dcf->polaris_service_namespace.len);
  std::string name_str(
    reinterpret_cast<char *>(dcf->polaris_service_name.data),
    dcf->polaris_service_name.len);

  std::string polaris_metadata_dir;
  if (dcf->polaris_dynamic_route_enabled) {
    polaris_metadata_dir = polaris_metadata_root_dir +
      namespace_str + "#" + name_str + "#metadata";
  } else {
    polaris_metadata_dir = polaris_metadata_route_meta_root_dir +
      namespace_str + "#" + name_str + "#metadata";
  }

  std::ifstream polaris_metadata_file(polaris_metadata_dir.c_str());
  std::string line;
  while (std::getline(polaris_metadata_file, line)) {
    std::string delimiter = " ";
    vector<string> vec;
    split_string(line, vec, delimiter);
    if (vec.size() == 2 && vec[1] != "") {
      ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0,
        "default value for metadata %s = %s", vec[0].c_str(), vec[1].c_str());
      name_key_list = name_key_list + vec[0] + "=" + vec[1] + ";";
    } else {
      name_key_list = name_key_list + line + ";";
    }
  }
}

void read_default_metadata_from_flie(ngx_conf_t *cf, ngx_http_upstream_polaris_srv_conf_t *dcf,
  std::string& default_key_list) {
  std::string polaris_default_metadata_dir;
  if (dcf->polaris_dynamic_route_enabled) {
    polaris_default_metadata_dir =
      polaris_metadata_root_dir + std::string("default#default#metadata");
  } else {
    polaris_default_metadata_dir =
      polaris_metadata_route_meta_root_dir + std::string("default#default#metadata");
  }
  std::ifstream polaris_default_metadata_file(polaris_default_metadata_dir.c_str());
  ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0,
        "default metadata file path: %s", polaris_default_metadata_dir.c_str());
  std::string line;
  while (std::getline(polaris_default_metadata_file, line)) {
    std::string delimiter = " ";
    vector<string> vec;
    split_string(line, vec, delimiter);
    if (vec.size() == 2 && vec[1] != "") {
      ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0,
        "default value for metadata in default file: %s = %s", vec[0].c_str(), vec[1].c_str());
      default_key_list = default_key_list + vec[0] + "=" + vec[1] + ";";
    } else {
      default_key_list = default_key_list + line + ";";
    }
  }
}

void read_default_fail_status_list_from_flie(ngx_conf_t *cf, ngx_http_upstream_polaris_srv_conf_t *dcf,
  std::string& default_key_list) {
  std::string polaris_default_fail_status_dir;
  polaris_default_fail_status_dir =
    polaris_fail_status_root_dir + std::string("default#default#fail_status");

  std::ifstream polaris_default_fail_status_file(polaris_default_fail_status_dir.c_str());
  ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0,
        "default fail_status file path: %s", polaris_default_fail_status_dir.c_str());
  std::string line;
  while (std::getline(polaris_default_fail_status_file, line)) {
    default_key_list = default_key_list + line + ";";
  }
}

static ngx_int_t ngx_http_upstream_init_polaris_peer(ngx_http_request_t *r,
                                                     ngx_http_upstream_srv_conf_t *us);

static ngx_int_t ngx_http_upstream_init_polaris(ngx_conf_t *cf, ngx_http_upstream_srv_conf_t *us) {
  ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cf->log, 0, "init polaris module");

  ngx_http_upstream_polaris_srv_conf_t *dcf =
      reinterpret_cast<ngx_http_upstream_polaris_srv_conf_t *>(
          ngx_http_conf_upstream_srv_conf(us, ngx_http_upstream_polaris_module));

  if (dcf->original_init_upstream(cf, us) != NGX_OK) {
    return NGX_ERROR;
  }

  dcf->original_init_peer = us->peer.init;
  us->peer.init           = ngx_http_upstream_init_polaris_peer;
  dcf->enabled            = 1;

  return NGX_OK;
}

static ngx_int_t ngx_http_upstream_get_polaris_peer(ngx_peer_connection_t *pc, void *data);

static void ngx_http_upstream_free_polaris_peer(ngx_peer_connection_t *pc, void *data,
                                                ngx_uint_t state);

static ngx_int_t ngx_http_upstream_init_polaris_peer(ngx_http_request_t *r,
                                                     ngx_http_upstream_srv_conf_t *us) {
  ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "init polaris peer");

  // 1 original init
  ngx_http_upstream_polaris_srv_conf_t *dcf =
      reinterpret_cast<ngx_http_upstream_polaris_srv_conf_t *>(
          ngx_http_conf_upstream_srv_conf(us, ngx_http_upstream_polaris_module));

  if (dcf->original_init_peer(r, us) != NGX_OK) {
    return NGX_ERROR;
  }

  // 2 init peer data
  ngx_http_upstream_polaris_peer_data_t *dp =
      reinterpret_cast<ngx_http_upstream_polaris_peer_data_t *>(
          ngx_palloc(r->pool, sizeof(ngx_http_upstream_polaris_peer_data_t)));
  if (dp == NULL) {
    return NGX_ERROR;
  }

  dp->conf               = dcf;
  dp->upstream           = r->upstream;
  dp->data               = r->upstream->peer.data;
  dp->original_get_peer  = r->upstream->peer.get;
  dp->original_free_peer = r->upstream->peer.free;
  dp->request            = r;

  r->upstream->peer.data = dp;
  r->upstream->peer.get  = ngx_http_upstream_get_polaris_peer;
  r->upstream->peer.free = ngx_http_upstream_free_polaris_peer;

  // control the retry times >= 2.
  if (r->upstream->peer.tries < 2) {
    r->upstream->peer.tries = 2;
    ngx_http_upstream_rr_peer_data_t *rrp =
        reinterpret_cast<ngx_http_upstream_rr_peer_data_t *>(dp->data);
    rrp->peers->single = 0;
  }

  // 3 init polaris params
  ngx_http_upstream_polaris_ctx_t *ctx = reinterpret_cast<ngx_http_upstream_polaris_ctx_t *>(
      ngx_http_get_module_ctx(r, ngx_http_upstream_polaris_module));

  if (ctx == NULL) {
    ctx = reinterpret_cast<ngx_http_upstream_polaris_ctx_t *>(
        ngx_palloc(r->pool, sizeof(ngx_http_upstream_polaris_ctx_t)));
    if (ctx == NULL) return NGX_ERROR;

    memset(ctx, 0, sizeof(ngx_http_upstream_polaris_ctx_t));
    ngx_http_set_ctx(r, ctx, ngx_http_upstream_polaris_module);
  }

  if (polaris_init_params(dcf, r, ctx) != NGX_OK) {
    return NGX_ERROR;
  }

  ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "init polaris param name:%s", ctx->name);

  return NGX_OK;
}

static ngx_int_t ngx_http_upstream_get_polaris_peer(ngx_peer_connection_t *pc, void *data) {
  ngx_http_upstream_polaris_peer_data_t *bp =
      reinterpret_cast<ngx_http_upstream_polaris_peer_data_t *>(data);
  // ngx_http_upstream_polaris_srv_conf_t   *dcf  = bp->conf;
  ngx_http_request_t *r = bp->request;
  // ngx_http_upstream_t               *u   = r->upstream;

  ngx_log_debug(NGX_LOG_DEBUG_HTTP, pc->log, 0, "get polaris peer, try: %ui", pc->tries);

  pc->cached     = 0;
  pc->connection = NULL;

  ngx_http_upstream_polaris_ctx_t *ctx = reinterpret_cast<ngx_http_upstream_polaris_ctx_t *>(
      ngx_http_get_module_ctx(r, ngx_http_upstream_polaris_module));

  int ret = polaris_get_addr(ctx);

  if (ret != 0) {
    ngx_log_debug(NGX_LOG_DEBUG_HTTP, pc->log, 0, "get polaris addr fail, use default server.");
    return bp->original_get_peer(pc, bp->data);
  }

  pc->sockaddr = (struct sockaddr *)&ctx->addr;
  pc->socklen  = sizeof(struct sockaddr_in);

  // pc->host = (ngx_str_t*)ngx_palloc(r->pool, sizeof(ngx_str_t));
  // pc->host->data = (u_char *)ctx->name;
  // pc->host->len  = strlen(ctx->name);

  pc->name = reinterpret_cast<ngx_str_t *>(ngx_palloc(r->pool, sizeof(ngx_str_t)));

  pc->name->len  = strlen(ctx->name);
  pc->name->data = reinterpret_cast<u_char *>(ngx_palloc(r->pool, pc->name->len));
  ngx_memcpy(pc->name->data, ctx->name, pc->name->len);

  return NGX_OK;
}

static void ngx_http_upstream_free_polaris_peer(ngx_peer_connection_t *pc, void *data,
                                                ngx_uint_t state) {
  ngx_http_upstream_polaris_peer_data_t *bp =
      reinterpret_cast<ngx_http_upstream_polaris_peer_data_t *>(data);

  ngx_log_debug(NGX_LOG_DEBUG_HTTP, pc->log, 0, "free polaris peer state:%d", state);

  ngx_http_upstream_polaris_ctx_t *ctx = reinterpret_cast<ngx_http_upstream_polaris_ctx_t *>(
      ngx_http_get_module_ctx(bp->request, ngx_http_upstream_polaris_module));

  ngx_log_debug(NGX_LOG_DEBUG_HTTP, pc->log, 0, "free polaris peer ret status code: %d",
    bp->request->headers_out.status);
  // free polaris peer if get_polaris_peer success
  if (ctx->polaris_ret == 0) {
    ctx->polaris_ret = state & NGX_PEER_FAILED ? -1 : 0;
    if (ctx->polaris_ret == 0 && ctx->polaris_fail_status_report_enabled) {
      std::string fail_report_status = std::string(
        reinterpret_cast<char *>(ctx->polaris_fail_status_list.data),
        ctx->polaris_fail_status_list.len);
      std::string delimiter = ";";
      vector<string> vec;
      split_string(fail_report_status, vec, delimiter);
      for (size_t i = 0; i < vec.size(); ++i) {
        if (vec[i] != "" && vec[i] == std::to_string(bp->request->headers_out.status)) {
          ngx_log_debug(NGX_LOG_DEBUG_HTTP, pc->log, 0, "fail status matched, report fail, code: %s", vec[i].c_str());
          ctx->polaris_ret = -1;
          break;
        }
      }
    }
    polaris_report(ctx);

    if (pc->tries) {
      pc->tries--;
    }

    return;
  }

  // free origin peer.
  bp->original_free_peer(pc, bp->data, state);
}

static void *ngx_http_upstream_polaris_create_conf(ngx_conf_t *cf) {
  ngx_http_upstream_polaris_srv_conf_t *conf =
      reinterpret_cast<ngx_http_upstream_polaris_srv_conf_t *>(
          ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_polaris_srv_conf_t)));

  if (conf == NULL) {
    return NULL;
  }

  ngx_str_set(&conf->polaris_service_namespace, "");
  ngx_str_set(&conf->polaris_service_name, "");
  ngx_str_set(&conf->polaris_lb_key, "");
  conf->polaris_lb_key_lengths = NULL;
  conf->polaris_lb_key_values = NULL;
  conf->polaris_service_namespace_lengths = NULL;
  conf->polaris_service_namespace_values = NULL;
  conf->polaris_timeout = 1;
  conf->polaris_lb_mode = 0;
  conf->polaris_dynamic_route_enabled = false;
  conf->metadata_route_failover_mode = 0;
  conf->polaris_metadata_route_enabled = false;
  ngx_str_set(&conf->polaris_fail_status_list, "");
  conf->polaris_fail_status_report_enabled = false;

  return conf;
}

// parse polaris configuration.
static char *ngx_http_upstream_polaris_set_handler(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
  ngx_http_upstream_srv_conf_t *uscf = reinterpret_cast<ngx_http_upstream_srv_conf_t *>(
      ngx_http_conf_get_module_srv_conf(cf, ngx_http_upstream_module));

  ngx_http_upstream_polaris_srv_conf_t *dcf =
      reinterpret_cast<ngx_http_upstream_polaris_srv_conf_t *>(
          ngx_http_conf_upstream_srv_conf(uscf, ngx_http_upstream_polaris_module));

  ngx_str_t *value = reinterpret_cast<ngx_str_t *>(cf->args->elts);

  ngx_http_script_compile_t sc;

  for (unsigned int i = 1; i < cf->args->nelts; ++i) {
    // polaris namespace
    if (ngx_strncmp(value[i].data, "service_namespace=", 18) == 0) {
      ngx_str_t s = {value[i].len - 18, &value[i].data[18]};

      int n = ngx_http_script_variables_count(&s);

      if (n) {
        ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

        sc.cf = cf;
        sc.source = &s;
        sc.lengths = &dcf->polaris_service_namespace_lengths;
        sc.values = &dcf->polaris_service_namespace_values;
        sc.variables = n;
        sc.complete_lengths = 1;
        sc.complete_values = 1;

        if (ngx_http_script_compile(&sc) != NGX_OK) {
          ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "ngx_http_script_compile:%V, failed", value[i]);
          return const_cast<char *>("ngx_http_script_compile failed");
        }
      } else {
        dcf->polaris_service_namespace = s;
      }
      if (dcf->polaris_service_namespace.len <= 0 &&
          dcf->polaris_service_namespace_lengths == NULL) {
        dcf->polaris_service_namespace.data = NULL;
        dcf->polaris_service_namespace.len = 0;
        return const_cast<char *>("invalid polaris namespace");
      }
      continue;
    }

    if (ngx_strncmp(value[i].data, "service_name=", 13) == 0) {
      ngx_str_t s = {value[i].len - 13, &value[i].data[13]};

      int n = ngx_http_script_variables_count(&s);

      if (n) {
        ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

        sc.cf = cf;
        sc.source = &s;
        sc.lengths = &dcf->polaris_service_name_lengths;
        sc.values = &dcf->polaris_service_name_values;
        sc.variables = n;
        sc.complete_lengths = 1;
        sc.complete_values = 1;

        if (ngx_http_script_compile(&sc) != NGX_OK) {
          ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "ngx_http_script_compile:%V, failed", value[i]);
          return const_cast<char *>("ngx_http_script_compile failed");
        }
      } else {
        dcf->polaris_service_name = s;
      }
      if (dcf->polaris_service_name.len <= 0 && dcf->polaris_service_name_lengths == NULL) {
        dcf->polaris_service_name.data = NULL;
        dcf->polaris_service_name.len = 0;
        return const_cast<char *>("invalid polaris name");
      }

      continue;
    }

    if (ngx_strncmp(value[i].data, "timeout=", 8) == 0) {
      ngx_str_t s = {value[i].len - 8, &value[i].data[8]};

      if (s.len > 0) {
        sscanf((const char *)s.data, "%f", &dcf->polaris_timeout);
      }

      if (s.len <= 0 || dcf->polaris_timeout <= 0) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "dcf->polaris_timeout:%lf invalid",
                           dcf->polaris_timeout);
        return const_cast<char *>("invalid timeout key");
      }
      continue;
    }

    if (ngx_strncmp(value[i].data, "mode=", 5) == 0) {
      ngx_str_t s = {value[i].len - 5, &value[i].data[5]};

      if (s.len > 0) {
        dcf->polaris_lb_mode = ngx_atoi(s.data, s.len);
      }

      if (dcf->polaris_lb_mode < 0 || dcf->polaris_lb_mode > 3) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0,
                          "dcf->polaris_lb_mode:%lf invalid, only valid in 0, 1, 2, 3",
                          dcf->polaris_lb_mode);
        return const_cast<char *>("invalid polaris lb mode");
      }
      continue;
    }

    if (ngx_strncmp(value[i].data, "key=", 4) == 0) {
      ngx_str_t s = {value[i].len - 4, &value[i].data[4]};

      int n = ngx_http_script_variables_count(&s);

      if (n) {
        ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

        sc.cf = cf;
        sc.source = &s;
        sc.lengths = &dcf->polaris_lb_key_lengths;
        sc.values = &dcf->polaris_lb_key_values;
        sc.variables = n;
        sc.complete_lengths = 1;
        sc.complete_values = 1;

        if (ngx_http_script_compile(&sc) != NGX_OK) {
          ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "ngx_http_script_compile:%V, failed", value[i]);
          return const_cast<char *>("ngx_http_script_compile failed");
        }
      } else {
        dcf->polaris_lb_key.len = value[i].len - 4;
        dcf->polaris_lb_key.data = reinterpret_cast<u_char*>(ngx_palloc(
                                                            cf->pool, dcf->polaris_lb_key.len));
        memcpy(dcf->polaris_lb_key.data, &value[i].data[4], dcf->polaris_lb_key.len);;
      }
      if (dcf->polaris_lb_key.len <= 0 && dcf->polaris_lb_key_lengths == NULL) {
        dcf->polaris_lb_key.data = NULL;
        dcf->polaris_lb_key.len = 0;
        return const_cast<char *>("invalid polaris key");
      }
      continue;
    }

    if (ngx_strncmp(value[i].data, "dr=", 3) == 0) {
      ngx_str_t s = {value[i].len - 3, &value[i].data[3]};
      if (ngx_strcmp(s.data, "on") == 0) {
        dcf->polaris_dynamic_route_enabled = true;
      } else {
        dcf->polaris_dynamic_route_enabled = false;
      }
      continue;
    }

    if (ngx_strncmp(value[i].data, "mr=", 3) == 0) {
      ngx_str_t s = {value[i].len - 3, &value[i].data[3]};
      if (ngx_strcmp(s.data, "on") == 0) {
        dcf->polaris_metadata_route_enabled = true;
      } else {
        dcf->polaris_metadata_route_enabled = false;
      }
      continue;
    }

    if (ngx_strncmp(value[i].data, "mr_mode=", 8) == 0) {
      ngx_str_t s = {value[i].len - 8, &value[i].data[8]};

      if (s.len > 0) {
        dcf->metadata_route_failover_mode = ngx_atoi(s.data, s.len);
      }

      if (dcf->metadata_route_failover_mode < 0 || dcf->metadata_route_failover_mode > 2) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0,
                          "dcf->metadata_route_failover_mode:%lf invalid, only valid in 0, 1, 2",
                          dcf->metadata_route_failover_mode);
        return const_cast<char *>("invalid polaris metadata route failover mode");
      }
      continue;
    }

    if (ngx_strncmp(value[i].data, "metadata=", 9) == 0) {
      ngx_str_t s = {value[i].len - 9, &value[i].data[9]};
      std::string metadata(reinterpret_cast<char *>(s.data), s.len);
      std::string polaris_metadata;

      for (size_t i = 0; i < metadata.size(); i++) {
        if (metadata[i] == '[' || metadata[i] == ']') {
          continue;
        } else if (metadata[i] == ':') {
          polaris_metadata += '=';
          continue;
        } else if (metadata[i] == ',') {
          polaris_metadata += ';';
          continue;
        }
        polaris_metadata += metadata[i];
      }
      polaris_metadata += ';';

      dcf->polaris_metadata_from_nginx_conf.len = polaris_metadata.length();
      dcf->polaris_metadata_from_nginx_conf.data = reinterpret_cast<u_char*>(
        ngx_palloc(cf->pool, dcf->polaris_metadata_from_nginx_conf.len));
      memcpy(dcf->polaris_metadata_from_nginx_conf.data, &polaris_metadata[0],
        dcf->polaris_metadata_from_nginx_conf.len);
      ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0, "metadata key:value list: %V",
        &dcf->polaris_metadata_from_nginx_conf);

      continue;
    }

    if (ngx_strncmp(value[i].data, "fail_report=", 12) == 0) {
      ngx_str_t s = {value[i].len - 12, &value[i].data[12]};
      std::string fail_status(reinterpret_cast<char *>(s.data), s.len);
      std::string polaris_fail_report_status;

      for (size_t i = 0; i < fail_status.size(); i++) {
        if (fail_status[i] == '[' || fail_status[i] == ']') {
          continue;
        } else if (fail_status[i] == ':') {
          polaris_fail_report_status += '=';
          continue;
        } else if (fail_status[i] == ',') {
          polaris_fail_report_status += ';';
          continue;
        }
        polaris_fail_report_status += fail_status[i];
      }
      polaris_fail_report_status += ';';

      dcf->polaris_fail_status_report_enabled = true;
      dcf->polaris_fail_status_list.len = polaris_fail_report_status.length();
      dcf->polaris_fail_status_list.data = reinterpret_cast<u_char*>(
        ngx_palloc(cf->pool, dcf->polaris_fail_status_list.len));
      memcpy(dcf->polaris_fail_status_list.data, &polaris_fail_report_status[0],
        dcf->polaris_fail_status_list.len);
      ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0, "fail report status list: %V",
        &dcf->polaris_fail_status_list);

      continue;
    }
  }

  dcf->enabled = true;

  if (dcf->polaris_dynamic_route_enabled && dcf->polaris_metadata_route_enabled) {
    return const_cast<char *>("dynamic route and metadata route can't be turned on at same time.");
  }

  if (dcf->polaris_dynamic_route_enabled) {
    std::string metadata_key_list;
    read_default_metadata_from_flie(cf, dcf, metadata_key_list);
    read_name_metadata_from_file(cf, dcf, metadata_key_list);

    dcf->polaris_dynamic_route_metadata_list.len = metadata_key_list.length();
    dcf->polaris_dynamic_route_metadata_list.data = reinterpret_cast<u_char*>(
      ngx_palloc(cf->pool, dcf->polaris_dynamic_route_metadata_list.len));
    memcpy(dcf->polaris_dynamic_route_metadata_list.data, &metadata_key_list[0],
      dcf->polaris_dynamic_route_metadata_list.len);
    ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0, "dynamic metadata key list: %V",
      &dcf->polaris_dynamic_route_metadata_list);
  }

  if (dcf->polaris_metadata_route_enabled) {
    std::string metadata_key_list;
    read_default_metadata_from_flie(cf, dcf, metadata_key_list);
    read_name_metadata_from_file(cf, dcf, metadata_key_list);

    dcf->polaris_metadata_route_metadata_list.len = metadata_key_list.length();
    dcf->polaris_metadata_route_metadata_list.data = reinterpret_cast<u_char*>(
      ngx_palloc(cf->pool, dcf->polaris_metadata_route_metadata_list.len));
    memcpy(dcf->polaris_metadata_route_metadata_list.data, &metadata_key_list[0],
      dcf->polaris_metadata_route_metadata_list.len);
    ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0, "metadata route meta key list: %V",
      &dcf->polaris_metadata_route_metadata_list);
  }

  if (!dcf->polaris_fail_status_report_enabled) {
    std::string fail_status_list;
    read_default_fail_status_list_from_flie(cf, dcf, fail_status_list);
    dcf->polaris_fail_status_report_enabled = true;
    dcf->polaris_fail_status_list.len = fail_status_list.length();
    dcf->polaris_fail_status_list.data = reinterpret_cast<u_char*>(
      ngx_palloc(cf->pool, dcf->polaris_fail_status_list.len));
    memcpy(dcf->polaris_fail_status_list.data, &fail_status_list[0],
      dcf->polaris_fail_status_list.len);
    ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0, "fail report status list from default file: %V",
      &dcf->polaris_fail_status_list);
  }

  if (dcf->original_init_upstream) {
    return const_cast<char *>("is duplicated");
  }

  dcf->original_init_upstream =
      uscf->peer.init_upstream ? uscf->peer.init_upstream : ngx_http_upstream_init_round_robin;

  uscf->peer.init_upstream = ngx_http_upstream_init_polaris;

  ngx_conf_log_error(
      NGX_LOG_NOTICE, cf, 0,
      "init service_namespace:%s, service_name:%s, timeout: %.2f, mode: %d, "
      "key: %s, dr: %d, mr_mode: %d, fail_status: %s",
      dcf->polaris_service_namespace.data, dcf->polaris_service_name.data, dcf->polaris_timeout,
      dcf->polaris_lb_mode, dcf->polaris_lb_key.data, dcf->polaris_dynamic_route_enabled,
      dcf->metadata_route_failover_mode, dcf->polaris_fail_status_list.data);

  return NGX_CONF_OK;
}
