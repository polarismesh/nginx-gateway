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

static const char KEY_NAMESPACE[] = "namespace=";
static const uint32_t KEY_NAMESPACE_SIZE = sizeof(KEY_NAMESPACE) - 1;
static const char KEY_SERVICE_NAME[] = "service=";
static const uint32_t KEY_SERVICE_NAME_SIZE = sizeof(KEY_SERVICE_NAME) - 1;
static const char DEFAULT_NAMESPACE[] = "default";
static const uint32_t DEFAULT_NAMESPACE_SIZE = sizeof(DEFAULT_NAMESPACE) - 1;

static const std::string LABEL_KEY_METHOD = "$method";
static const std::string LABEL_KEY_HEADER = "$header.";
static const std::string LABEL_KEY_QUERY = "$query.";
static const std::string LABEL_KEY_CALLER_IP = "$caller_ip";
static const std::string PATH_SBIN = "sbin";

static bool endsWith(const std::string& str, const std::string& suffix)
{
    return str.size() >= suffix.size() && 0 == str.compare(str.size()-suffix.size(), suffix.size(), suffix);
}

static std::string get_polaris_conf_path() {
  char *cwd = get_current_dir_name();
  std::string dirname(cwd);
  free(cwd);
  if (endsWith(dirname, PATH_SBIN)) {
     dirname = dirname.substr(0, dirname.rfind(PATH_SBIN));
  } else {
     dirname += "/";
  }
  dirname += "conf/polaris.yaml";
  return dirname;
}

class LimitApiWrapper {
 public:
  LimitApiWrapper() { m_limit = polaris::LimitApi::CreateFromFile(get_polaris_conf_path()); }

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