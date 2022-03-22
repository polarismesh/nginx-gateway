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
#include "polaris/log.h"
#include "polaris/limit.h"

#ifndef POLARIS_CONFIG_DIR
#define POLARIS_CONFIG_DIR "polaris.yaml"
#endif

#ifndef POLARIS_LOG_DIR
#define POLARIS_LOG_DIR "/polaris"
#endif


static const char kServiceNamespace[] = "service_namespace=";
static const uint32_t kServiceNamespaceSize = sizeof(kServiceNamespace) - 1;
static const char kServiceName[] = "service_name=";
static const uint32_t kServiceNameSize = sizeof(kServiceName) - 1;
static const char kTimeout[] = "timeout=";
static const uint32_t kTimeoutSize = sizeof(kTimeout) - 1;

class LimitApiWrapper {
 public:
  LimitApiWrapper() { 
    polaris::SetLogDir(POLARIS_LOG_DIR);
    m_limit = polaris::LimitApi::CreateFromFile(POLARIS_CONFIG_DIR); 
  }

  static LimitApiWrapper& Instance() {
    static LimitApiWrapper limit_api;
    return limit_api;
  }

  polaris::LimitApi* GetLimitApi() { return m_limit; }

 private:
  polaris::LimitApi* m_limit;
};

#define Limit_API_SINGLETON LimitApiWrapper::Instance()

#endif  // NGINX_MODULE_POLARIS_NGINX_POLARIS_LIMIT_MODULE_NGX_HTTP_POLARIS_LIMIT_MODULE_H_