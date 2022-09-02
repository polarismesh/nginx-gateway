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

#ifndef NGINX_MODULE_POLARIS_NGINX_POLARIS_LIMIT_MODULE_NGX_HTTP_POLARIS_LIMIT_MODULE_H_
#define NGINX_MODULE_POLARIS_NGINX_POLARIS_LIMIT_MODULE_NGX_HTTP_POLARIS_LIMIT_MODULE_H_

extern "C" {
    #include <ngx_config.h>
    #include <ngx_core.h>
    #include <ngx_http.h>
}

#include "polaris/limit.h"
#include <iostream>
#include <string>
#include <unistd.h>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <mutex>

static const char KEY_ENABLE[] = "enable";
static const uint32_t KEY_ENABLE_SIZE = sizeof(KEY_ENABLE) - 1;
static const char KEY_NAMESPACE[] = "namespace=";
static const uint32_t KEY_NAMESPACE_SIZE = sizeof(KEY_NAMESPACE) - 1;
static const char KEY_SERVICE_NAME[] = "service=";
static const uint32_t KEY_SERVICE_NAME_SIZE = sizeof(KEY_SERVICE_NAME) - 1;

static const std::string ENV_NAMESPACE = "polaris_nginx_namespace";
static const std::string ENV_SERVICE = "polaris_nginx_service";
static const std::string ENV_RATELIMIT_ENABLE = "polaris_nginx_ratelimit_enable";

static const std::string DEFAULT_NAMESPACE = "default";
static const std::string DEFAULT_SERVICE = "nginx-gateway";

static const std::string LABEL_KEY_METHOD = "$method";
static const std::string LABEL_KEY_HEADER = "$header.";
static const std::string LABEL_KEY_QUERY = "$query.";
static const std::string LABEL_KEY_CALLER_IP = "$caller_ip";
static const std::string PATH_SBIN = "sbin";
static const std::string DEFAULT_POLARIS_LOG_DIR = "/tmp/polaris";

class LimitApiWrapper {
 public:

  LimitApiWrapper() {
    m_created = false;
  }

  void LoadPolarisConfig();

  void Init(ngx_log_t *ngx_log);

  static LimitApiWrapper& Instance() {
    static LimitApiWrapper limit_api;
    return limit_api;
  }

  polaris::LimitApi* GetLimitApi(ngx_log_t *ngx_log) { 
    if (m_created) {
      return m_limit;
    }
    m_mtx.lock();
    if (m_created) {
      return m_limit;
    }
    Init(ngx_log);
    m_mtx.unlock();
    return m_limit;
  }

 private:
  polaris::LimitApi* m_limit;
  std::string m_polaris_config;
  std::mutex m_mtx;
  bool m_created;
};

#define Limit_API_SINGLETON LimitApiWrapper::Instance()

#endif  // NGINX_MODULE_POLARIS_NGINX_POLARIS_LIMIT_MODULE_NGX_HTTP_POLARIS_LIMIT_MODULE_H_