# Nginx网关

---

## 什么是nginx网关

Nginx网关是一个微服务网关。在原生Nginx的基础上，通过对接北极星服务治理中心，扩展nginx的module，提供微服务场景下所需要使用的增强能力，如对接注册中心、访问限流、流量调度、熔断降级、动态配置等。

## 支持的nginx版本

当前插件集成的版本为```nginx-1.23.1```

## 快速入门

快速入门指南可以参考：[nginx网关快速入门](https://polarismesh.cn/docs/%E4%BD%BF%E7%94%A8%E6%8C%87%E5%8D%97/%E7%BD%91%E5%85%B3/%E4%BD%BF%E7%94%A8nginx/)

## 编译指南

用户也可以通过源码编译的方式，生成安装包。

- 安装依赖项：在编译之前，需要先安装依赖项。通过执行```yum install autoconf automake libtool curl make gcc-c++ libstdc++-devel unzip```进行安装。

- 下载源码包：可以直接从[releases](https://github.com/polarismesh/nginx-gateway/releases)下载最新的nginx源码包。

- 编译安装：解压源码包。执行以下命令构建，构建过程需要连接网络下载依赖，请务必保证外网连通性。

  ```shell
  cd build
  bash build.sh
  ```

- 获取编译结果：构建成功后，在源码包的根目录下，可以获取安装包：```nginx-gateway-release-*.tar.gz```
