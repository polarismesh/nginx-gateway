# nginx-polaris
## Table of Contents

* [What is nginx-polaris?](#what-is-nginx-polaris)
* [Supported Nginx Version](#supported-nginx-version)
* [Installation Instructions](#installation-instructions)
* [Features And Usage](#features-and-usages)
* [Directives Explanation](#directives-explanation)

---

## What is nginx-polaris?
nginx-polaris is a nginx module which works as a load-balanceer based on PolarisMesh. No hardcoded ip is remained after embedding nginx-polaris with your nginx, which makes your nginx configuration file easy to maintain. What's more, nginx-polaris provided various functionality which PolarisMesh can provide.

## Supported Nginx Version
nginx version above 1.4.7 will have no problem.

## Installation Instructions
1. First you need to setup a [PolarisMesh server](https://github.com/PolarisMesh/polaris).
2. Compile and package [PolarisCpp](https://github.com/PolarisMesh/polaris-cpp). Unzip `polaris_cpp_sdk.tar.gz` and move the whole folder to your compile workspace, such as `/path/to/workspace/polaris_cpp_sdk`
3. Move file `config` to `/path/to/workspace/polaris_cpp_sdk`, move folder `nginx_polaris_limit_module` and `nginx_polaris_module` to `/path/to/workspace`.
4. Download opensource [Nginx](http://nginx.org/en/download.html) to your workspace, so the final file structure of your workspace looks like this:
```
|-- nginx-1.19.2
|   |-- CHANGES
|   |-- CHANGES.ru
|   |-- LICENSE
|   |-- Makefile
|   |-- README
|   |-- auto
|   |-- build
|   |-- build.sh
|   |-- conf
|   |-- configure
|   |-- contrib
|   |-- html
|   |-- man
|   |-- objs
|   |-- polaris_client
|   `-- src
|-- nginx_polaris_limit_module
|   |-- config
|   |-- ngx_http_polaris_limit_module.cpp
|   `-- ngx_http_polaris_limit_module.h
|-- nginx_polaris_module
|   |-- config
|   |-- ngx_http_upstream_polaris_module.cpp
|   |-- ngx_http_upstream_polaris_module.h
|   |-- ngx_http_upstream_polaris_wrapper.cpp
|   `-- ngx_stream_upstream_polaris_module.cpp
`-- polaris_cpp_sdk
    |-- config
    |-- dlib
    |-- include
    `-- slib
```
5. Run following command to build nginx with nginx-polaris module.
```
./configure \
        --add-module=/path/to/workspace/nginx_polaris_limit_module \
        --add-module=/path/to/workspace/nginx_polaris_module \
        --add-module=/path/to/workspace/polaris_cpp_sdk \
    --with-stream
make install
```

## Features And Usages
First you need to create a name by Polaris Server, we will take `Test:test.servicename` as an example, in which `Test` represents Polaris namespace and `test.servicename` represents Polaris name.
### L4 and L7 load balance.
Add below to your nginx configuration under http or stream namespace, L4 or L7 reverse proxy is ready to go and you are no longer bothered by hardcoded ip. 
```
upstream test_upstream {
    polaris service_namespace=Test service_name=test.servicename timeout=1.5;
    server 10.223.130.162:6000;
}
```
You can also use nginx native variable for runtime configuration like below. ***This feature is supported only in L4 load balance***
```
upstream test_upstream {
    polaris service_namespace=$http_namespace service_name=$http_name timeout=1.5;
    server 10.223.130.162:6000;
}
```
### Business-Level Fail Status Report
If you have already read the doc of [PolarisCpp](https://github.com/PolarisMesh/polaris-cpp), you will find out that Polaris has circuit breaker feature which relies on the business-level report on failed requests. Nginx-polaris has already report failed requests which are led by network error. In order to make it better, Nginx-polaris also supports business-level failed requests report, this feature is only activated in L4 load balance mode and developer needs to configure failed status code. For the configuration below, polaris Nginx module will report fail when backend server returns ***502*** or ***405*** to Nginx.
```
upstream test_upstream {
    polaris service_namespace=Test service_name=test.servicename timeout=1.5 fail_report=502,405;
    server 10.223.130.162:6000;
}
```
### Session Sticky
Nginx-polaris module supports various load balance mode. Developer can use Nginx-polaris module to achieve session sticky. Add configuration like below, session sticky is achieved and all the requests from same remote_addr will be proxied to the same backed server.
```
upstream test_upstream {
    polaris service_namespace=Test service_name=test.servicename timeout=1.5 mode=2 key=$remote_addr;
    server 10.223.130.162:6000;
}
```
### Dynamic Route
This feature is only supported in L4 load balance.
1. You need to learn about what dynamic route is from [PolarisCpp](https://github.com/PolarisMesh/polaris-cpp) first.
2. Tured on dynamic route by set ***dr=on***.
3. Set ***metadata*** key-value list to filter the value specified by http header, url arguments or cookies.
4. For example, if you configured dynamic route rules on Polaris Console like below:
![image](https://user-images.githubusercontent.com/7314436/130551250-c4b52235-59c6-456d-832b-74263f6a4ebd.png)
And you add upstream configuration like below:
```
upstream test_upstream {
    polaris service_namespace=Test service_name=polaris.demo timeout=1.5 dr=on metadata=[key1,key2];
    server 127.0.0.1:8000;
}
```
For all three kinds of requests like below, will be routed to instances which have ***test:true*** metadata.
```
POST /recv?key1=check HTTP/1.1
Host: 127.0.0.1
{
}
```
```
POST /recv HTTP/1.1
key1: check
Host: 127.0.0.1
{
}
```
```
POST /recv HTTP/1.1
Cookie: key1=check; x_host_key=176d798f8b4-c8c30f5e7d8ebe8a1bab15661f32730ecfb156e5;
Host: 127.0.0.1
{
}
```
Likewise, for all three kinds of requests like below, will be routed to instances which have ***test:false*** metadata.
```
POST /recv?key2=nonce HTTP/1.1
Host: 127.0.0.1
{
}
```
```
POST /recv HTTP/1.1
key2: nonce
Host: 127.0.0.1
{
}
```
```
POST /recv HTTP/1.1
Cookie: key2=nonce; x_host_key=176d798f8b4-c8c30f5e7d8ebe8a1bab15661f32730ecfb156e5;
Host: 127.0.0.1
{
}
```

## Directives Explanation

