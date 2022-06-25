/*
 * Copyright (C) 2020-2025 Tencent Limited
 * author: jasonygyang@tencent.com
 */

extern "C" {
    #include <ngx_config.h>
    #include <ngx_core.h>
    #include <ngx_http.h>
    #include <ngx_stream.h>
}

#include <time.h>
#include "ngx_http_upstream_polaris_module.h"

class ConsumerApiWrapperStream {
 public:
  ConsumerApiWrapperStream() { 
    polaris::SetLogDir(POLARIS_LOG_DIR);
    m_consumer = polaris::ConsumerApi::CreateFromFile(POLARIS_CONFIG_DIR); 
}

  static ConsumerApiWrapperStream& Instance() {
    static ConsumerApiWrapperStream consumer_api;
    return consumer_api;
  }

  polaris::ConsumerApi* GetConsumerApi() { return m_consumer; }

 private:
  polaris::ConsumerApi* m_consumer;
};

#define CONSUMER_API_SINGLETON_STREAM ConsumerApiWrapperStream::Instance()

typedef struct {
  ngx_int_t enabled;
  ngx_str_t polaris_service_namespace;
  ngx_array_t *polaris_service_namespace_lengths;
  ngx_array_t *polaris_service_namespace_values;

  ngx_str_t polaris_service_name;
  ngx_array_t *polaris_service_name_lengths;
  ngx_array_t *polaris_service_name_values;

  ngx_str_t polaris_set;

  float polaris_timeout;

  ngx_str_t polaris_lb_key;
  ngx_int_t polaris_lb_mode;
  ngx_array_t *polaris_lb_key_lengths;
  ngx_array_t *polaris_lb_key_values;

  ngx_int_t polaris_dynamic_route_enabled;
  ngx_str_t polaris_dynamic_route_metadata_list;

  ngx_http_upstream_init_pt original_init_upstream;
  ngx_http_upstream_init_peer_pt original_init_peer;
} ngx_stream_upstream_polaris_srv_conf_t;


typedef struct {
    /* the round robin data must be first */
    ngx_stream_upstream_rr_peer_data_t  rrp;
    ngx_stream_upstream_polaris_srv_conf_t*      polaris_conf;

    ngx_time_t                       polaris_start;
    char                             polaris_name[32];
    char                             polaris_ip[32];
    int                              polaris_port;
    ngx_str_t                        name;
    char                             instance_id[64];
    struct sockaddr_in               peer_addr;
    int                              polaris_ret;


    ngx_event_get_peer_pt            get_rr_peer;
    ngx_event_free_peer_pt           free_rr_peer;
    ngx_event_notify_peer_pt         notify_rr_peer;
} ngx_stream_upstream_polaris_peer_data_t;



static ngx_int_t ngx_stream_upstream_init_polaris_peer(ngx_stream_session_t *s,
    ngx_stream_upstream_srv_conf_t *us);
static ngx_int_t ngx_stream_upstream_get_polaris_peer(ngx_peer_connection_t *pc, void *data);
static void ngx_stream_upstream_free_polaris_peer(ngx_peer_connection_t *pc,
    void *data, ngx_uint_t state);
static void ngx_stream_upstream_notify_polaris_peer(ngx_peer_connection_t *pc,
    void *data, ngx_uint_t state);

static char *ngx_stream_upstream_polaris_handler(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

static ngx_command_t  ngx_stream_upstream_polaris_commands[] = {
    { ngx_string("polaris"),
      NGX_STREAM_UPS_CONF|NGX_CONF_1MORE,
      ngx_stream_upstream_polaris_handler,
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};

void * ngx_stream_upstream_polaris_create_main_conf(ngx_conf_t *cf) {
    ngx_stream_upstream_polaris_srv_conf_t* polaris_conf =
            reinterpret_cast<ngx_stream_upstream_polaris_srv_conf_t*>(ngx_pcalloc(
                cf->pool, sizeof(ngx_stream_upstream_polaris_srv_conf_t)));

    if (polaris_conf == NULL) {
        return NULL;
    }

    ngx_str_set(&polaris_conf->polaris_service_namespace, "");
    ngx_str_set(&polaris_conf->polaris_service_name, "");
    ngx_str_set(&polaris_conf->polaris_set, "");
    ngx_str_set(&polaris_conf->polaris_lb_key, "");
    polaris_conf->polaris_lb_key_lengths = NULL;
    polaris_conf->polaris_lb_key_values = NULL;
    polaris_conf->polaris_timeout = 1;
    polaris_conf->polaris_lb_mode = 0;
    polaris_conf->polaris_dynamic_route_enabled = false;

    return polaris_conf;
}

static ngx_stream_module_t  ngx_stream_upstream_polaris_module_ctx = {
    NULL,
    NULL,
    NULL,
    NULL,
    ngx_stream_upstream_polaris_create_main_conf,
    NULL,
};

extern "C" {
ngx_module_t  ngx_stream_upstream_polaris_module = {
    NGX_MODULE_V1,
    &ngx_stream_upstream_polaris_module_ctx,  /* module context */
    ngx_stream_upstream_polaris_commands,     /* module directives */
    NGX_STREAM_MODULE,                        /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};
}

ngx_int_t ngx_stream_upstream_init_polaris(ngx_conf_t *cf, ngx_stream_upstream_srv_conf_t *us) {
    if (ngx_stream_upstream_init_round_robin(cf, us) != NGX_OK) {
        return NGX_ERROR;
    }

    us->peer.init = ngx_stream_upstream_init_polaris_peer;

    return NGX_OK;
}


static ngx_int_t
ngx_stream_upstream_init_polaris_peer(ngx_stream_session_t *s,
    ngx_stream_upstream_srv_conf_t *us) {
    ngx_stream_upstream_polaris_srv_conf_t *polaris_conf;
    ngx_stream_upstream_polaris_peer_data_t *iphp;
    iphp = reinterpret_cast<ngx_stream_upstream_polaris_peer_data_t*>(
        ngx_palloc(s->connection->pool, sizeof(ngx_stream_upstream_polaris_peer_data_t)));
    if (iphp == NULL) {
        return NGX_ERROR;
    }

    s->upstream->peer.data = &iphp->rrp;

    if (ngx_stream_upstream_init_round_robin_peer(s, us) != NGX_OK) {
        return NGX_ERROR;
    }

    polaris_conf = reinterpret_cast<ngx_stream_upstream_polaris_srv_conf_t*>(
        ngx_stream_conf_upstream_srv_conf(us, ngx_stream_upstream_polaris_module));

    iphp->polaris_conf = polaris_conf;

    iphp->get_rr_peer = s->upstream->peer.get;
    iphp->free_rr_peer = s->upstream->peer.free;
    iphp->notify_rr_peer = s->upstream->peer.notify;

    s->upstream->peer.get  = ngx_stream_upstream_get_polaris_peer;
    s->upstream->peer.free = ngx_stream_upstream_free_polaris_peer;
    s->upstream->peer.notify = ngx_stream_upstream_notify_polaris_peer;

    return NGX_OK;
}


static ngx_int_t
ngx_stream_upstream_get_polaris_peer(ngx_peer_connection_t *pc, void *data) {
    ngx_stream_upstream_polaris_peer_data_t* iphp
            = reinterpret_cast<ngx_stream_upstream_polaris_peer_data_t*>(data);

    std::string serviceNameSpace(
        reinterpret_cast<char*>(iphp->polaris_conf->polaris_service_namespace.data),
            iphp->polaris_conf->polaris_service_namespace.len);
    std::string serviceName(reinterpret_cast<char*>(
        iphp->polaris_conf->polaris_service_name.data),
        iphp->polaris_conf->polaris_service_name.len);
    std::string polarisSet(reinterpret_cast<char*>(iphp->polaris_conf->polaris_set.data),
        iphp->polaris_conf->polaris_set.len);
    polaris::ServiceKey serviceKey = {serviceNameSpace, serviceName};
    polaris::Instance instance;
    polaris::GetOneInstanceRequest request(serviceKey);
    request.SetTimeout(iphp->polaris_conf->polaris_timeout);

    polaris::ServiceInfo service_info;
    service_info.metadata_["set"] = polarisSet;
    request.SetSourceService(service_info);

    polaris::ReturnCode ret =
        CONSUMER_API_SINGLETON_STREAM.GetConsumerApi()->GetOneInstance(request, instance);
    iphp->polaris_ret = ret;

    memset(&iphp->peer_addr, 0, sizeof(struct sockaddr_in));

    if (ret == polaris::kReturnOk) {
        iphp->peer_addr.sin_family = AF_INET;
        iphp->peer_addr.sin_port = htons(instance.GetPort());
        iphp->peer_addr.sin_addr.s_addr = inet_addr(instance.GetHost().c_str());
        snprintf(iphp->instance_id, sizeof(iphp->instance_id), "%s", instance.GetId().c_str());
        pc->sockaddr = reinterpret_cast<sockaddr*>(&(iphp->peer_addr));
        pc->socklen  = sizeof(struct sockaddr_in);
    } else {
        iphp->get_rr_peer(pc, &iphp->rrp);
        ngx_log_error(NGX_LOG_ERR, pc->log, 0,
                  "polaris get instance fail, namespace: %s, name: %s, ret: %d",
                  serviceNameSpace.c_str(), serviceName.c_str(), ret);
    }

    return NGX_OK;
}


static void ngx_stream_upstream_free_polaris_peer(
    ngx_peer_connection_t *pc, void *data, ngx_uint_t state) {
    ngx_stream_upstream_polaris_peer_data_t* iphp
            = reinterpret_cast<ngx_stream_upstream_polaris_peer_data_t*>(data);

    // 如果获取polaris成功，但是链接失败，是不会走到iphp缓存的get_peer的，会导致后续的free出现coredump
    int pre_ret = iphp->polaris_ret;

    if (pre_ret == 0) {
        // update ret
        iphp->polaris_ret = state & NGX_PEER_FAILED ? -1 : iphp->polaris_ret;

        std::string serviceNameSpace(
            reinterpret_cast<char*>(iphp->polaris_conf->polaris_service_namespace.data));
        std::string serviceName(
            reinterpret_cast<char*>(iphp->polaris_conf->polaris_service_name.data));
        std::string instanceId(iphp->instance_id);
        float time_out = (ngx_cached_time->sec - iphp->polaris_start.sec) * 1000 +
                        (ngx_cached_time->msec - iphp->polaris_start.msec);

        polaris::ServiceCallResult result;
        result.SetServiceNamespace(serviceNameSpace);
        result.SetServiceName(serviceName);
        result.SetInstanceId(instanceId);
        result.SetDelay(time_out);

        if (iphp->polaris_ret == 0) {
            result.SetRetCode(0);
            result.SetRetStatus(polaris::kCallRetOk);
        } else {
            result.SetRetCode(iphp->polaris_ret);
            result.SetRetStatus(polaris::kCallRetError);
        }

        polaris::ReturnCode ret_code =
            CONSUMER_API_SINGLETON_STREAM.GetConsumerApi()->UpdateServiceCallResult(result);
        if (ret_code != polaris::kReturnOk) {
            ngx_log_error(NGX_LOG_ERR, pc->log, 0,
                        "update call result for instance with error:%s, instance id: %s",
                        polaris::ReturnCodeToMsg(ret_code).c_str(), instanceId.c_str());
        }

        return;
    }

    iphp->free_rr_peer(pc, &iphp->rrp, state);
}

static void ngx_stream_upstream_notify_polaris_peer(
    ngx_peer_connection_t *pc, void *data, ngx_uint_t state) {
    ngx_stream_upstream_polaris_peer_data_t* iphp
            = reinterpret_cast<ngx_stream_upstream_polaris_peer_data_t*>(data);

    // update ret
    iphp->polaris_ret = state & NGX_PEER_FAILED ? -1 : iphp->polaris_ret;
    if (iphp->polaris_ret < 0) {
        iphp->notify_rr_peer(pc, &iphp->rrp, state);
    }
}

static char *
ngx_stream_upstream_polaris_handler(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    // init args
    ngx_stream_upstream_polaris_srv_conf_t* polaris_conf =
        reinterpret_cast<ngx_stream_upstream_polaris_srv_conf_t*>(conf);

    ngx_str_t* value = reinterpret_cast<ngx_str_t*>(cf->args->elts);
    for (unsigned int i = 1; i< cf->args->nelts; ++i) {
        // polaris namespace
        if (ngx_strncmp(value[i].data, "service_namespace=", 18) == 0) {
            ngx_str_t s = {value[i].len - 18, &value[i].data[18]};

            polaris_conf->polaris_service_namespace = s;

            continue;
        }

        if (ngx_strncmp(value[i].data, "service_name=", 13) == 0) {
            ngx_str_t s = {value[i].len - 13, &value[i].data[13]};

            polaris_conf->polaris_service_name = s;

            continue;
        }

        if (ngx_strncmp(value[i].data, "timeout=", 8) == 0) {
            ngx_str_t s = {value[i].len - 8, &value[i].data[8]};

            if (s.len > 0) {
                sscanf((const char *)s.data, "%f", &polaris_conf->polaris_timeout);
            }

            if (s.len <= 0 || polaris_conf->polaris_timeout <= 0) {
                ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "polaris_conf->polaris_timeout:%lf invalid",
                                polaris_conf->polaris_timeout);
                return const_cast<char *>("invalid timeout key");
            }
            continue;
        }

        if (ngx_strncmp(value[i].data, "mode=", 5) == 0) {
            ngx_str_t s = {value[i].len - 5, &value[i].data[5]};

            if (s.len > 0) {
                polaris_conf->polaris_lb_mode = ngx_atoi(s.data, s.len);
            }

            if (polaris_conf->polaris_lb_mode < 0 || polaris_conf->polaris_lb_mode > 3) {
                ngx_conf_log_error(NGX_LOG_ERR, cf, 0,
                    "polaris_conf->polaris_lb_mode:%lf invalid, only valid in 0, 1, 2, 3",
                    polaris_conf->polaris_lb_mode);
                return const_cast<char *>("invalid polaris lb mode");
            }
            continue;
        }

        if (ngx_strncmp(value[i].data, "set=", 4) == 0) {
            ngx_str_t s = {value[i].len - 4, &value[i].data[4]};

            polaris_conf->polaris_set = s;

            continue;
        }
    }

    ngx_stream_upstream_srv_conf_t* uscf =
            reinterpret_cast<ngx_stream_upstream_srv_conf_t*>(
                ngx_stream_conf_get_module_srv_conf(cf, ngx_stream_upstream_module));
    if (uscf->peer.init_upstream) {
        ngx_conf_log_error(NGX_LOG_WARN, cf, 0, "load balancing method redefined");
    }

    uscf->peer.init_upstream = ngx_stream_upstream_init_polaris;

    uscf->flags = NGX_STREAM_UPSTREAM_CREATE
                  |NGX_STREAM_UPSTREAM_WEIGHT
                  |NGX_STREAM_UPSTREAM_MAX_CONNS
                  |NGX_STREAM_UPSTREAM_MAX_FAILS
                  |NGX_STREAM_UPSTREAM_FAIL_TIMEOUT
                  |NGX_STREAM_UPSTREAM_DOWN;

    ngx_conf_log_error(
        NGX_LOG_NOTICE, cf, 0,
        "init service_namespace:%s, service_name:%s, polaris_set: %s, timeout: %.2f, mode: %d, key: %s, dr: %d",
        polaris_conf->polaris_service_namespace.data, polaris_conf->polaris_service_name.data,
        polaris_conf->polaris_set.data, polaris_conf->polaris_timeout,
        polaris_conf->polaris_lb_mode, polaris_conf->polaris_lb_key.data,
        polaris_conf->polaris_dynamic_route_enabled);

    return NGX_CONF_OK;
}