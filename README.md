# nginx-polaris
## Table of Contents

* [What is nginx-polaris?](#what-is-nginx-polaris)
* [Supported Nginx Version](#supported-nginx-version)
* [Installation Instructions](#installation-instructions)
* [Features And Usage](#features-and-usages)

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

