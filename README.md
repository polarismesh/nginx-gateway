# Nginx网关

---

## 什么是nginx网关

Nginx网关是一个微服务网关。在原生Nginx的基础上，通过对接北极星服务治理中心，扩展nginx的module，提供微服务场景下所需要使用的增强能力，如对接注册中心、访问限流、流量调度、熔断降级、动态配置等。

## 支持的nginx版本

当前支持 nginx 的版本为>=1.4.7

## 安装说明

### 操作系统支持

当前仅支持linux操作系统，linux下要求GCC版本>=4.8.5

```
[sam@VM_15_118_centos ~/testgen]$ gcc --version
gcc (GCC) 4.8.5 20150623 (Red Hat 4.8.5-39)
Copyright (C) 2015 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
```

### 下载安装包

可以直接从[releases](https://github.com/polarismesh/nginx-gateway/releases)下载最新的nginx网关安装包。

### 【可选】编译生成安装包

用户也可以通过源码编译的方式，生成安装包。

- 安装依赖项：在编译之前，需要先安装依赖项。通过执行```yum install autoconf automake libtool curl make gcc-c++ libstdc++-devel unzip```进行安装。

- 下载源码包：可以直接从[releases](https://github.com/polarismesh/nginx-gateway/releases)下载最新的nginx网关源码包。

- 编译安装：解压源码包。执行以下命令构建，构建过程需要连接网络下载依赖，请务必保证外网连通性。

  ```shell
  cd build
  bash build.sh
  ```

- 获取编译结果：构建成功后，在源码包的根目录下，可以获取安装包：```nginx-gateway-release-*.tar.gz```

### 【可选】修改端口号

nginx默认端口号为80，如需修改端口号，可以通过编辑conf/nginx.conf配置文件进行修改。

```
http {
  server {
    listen 80; #这里修改成希望监听的端口号
  }
}  
```

### 运行安装包

解压安装包，并运行命令启动nginx网关。

```
tar xf nginx-gateway-release-*.tar.gz
cd nginx-gateway-release-*/sbin
bash start.sh
```

## 使用指南

### 使用访问限流功能

- 安装北极星

  参考[安装指南](https://polarismesh.cn/zh/doc/%E5%BF%AB%E9%80%9F%E5%85%A5%E9%97%A8/%E5%AE%89%E8%A3%85%E6%9C%8D%E5%8A%A1%E7%AB%AF/%E5%AE%89%E8%A3%85%E5%8D%95%E6%9C%BA%E7%89%88.html#%E5%8D%95%E6%9C%BA%E7%89%88%E5%AE%89%E8%A3%85)进行安装。

- 配置限流规则

  在北极星可视化控制台上配置限流规则，比如针对uri为```/test```的请求，限制每秒只能100QPS。参考[限流使用指南](https://polarismesh.cn/zh/doc/%E4%BD%BF%E7%94%A8%E6%8C%87%E5%8D%97/%E8%AE%BF%E9%97%AE%E9%99%90%E6%B5%81/%E5%8D%95%E6%9C%BA%E9%99%90%E6%B5%81.html#%E5%8D%95%E6%9C%BA%E9%99%90%E6%B5%81)。

- 修改nginx配置

  通过修改nginx中的location，添加polaris_rate_limiting配置，启用北极星限流功能。
  ```
  http {
    server {
      location / {
        polaris_rate_limiting namespace=default service=testcpp;
      }
    }
  }
  ```

  polaris_rate_limiting参数说明：

  | 参数名    | 说明                             | 是否必选          |
  | --------- | -------------------------------- | ----------------- |
  | service   | 服务名，与限流规则服务名对应     | 是                |
  | namespace | 命名空间，与限流规则命名空间对应 | 否，不填为default |
  
- 重启nginx

  ```
  cd nginx-gateway-release-*/sbin
  bash stop.sh
  bash start.sh
  ```

