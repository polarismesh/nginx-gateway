FROM debian:stable-slim

# install dependencies
RUN set -ex \
    && apt-get update \
    && apt-get install --no-install-recommends --no-install-suggests -y \
     telnet curl net-tools vim \
     autoconf automake wget git libtool curl make gcc g++ unzip libpcre3 libpcre3-dev zlib1g zlib1g-dev libssl-dev

COPY source /build/source
COPY third_party /build/third_party
COPY polaris.yaml /build/

# build polaris-cpp
RUN set -ex \
    && cd /build/third_party \
    && git config --global http.sslverify false \
    && git clone -b release_v1.1.0 https://github.com/polarismesh/polaris-cpp polaris-cpp \
    && cd polaris-cpp \
    && make \
    && make package \
    && tar xf polaris_cpp_sdk.tar.gz \
    && mv polaris_cpp_sdk/* /build/third_party/polaris_client/ \
    && cd .. \
    && rm -rf polaris-cpp

RUN set -ex \
    && mkdir -p /etc/nginx \
    && cd /build/third_party \
    && ngx_file_name=nginx-1.23.1 \
    && curl http://nginx.org/download/"$ngx_file_name".tar.gz -o "$ngx_file_name".tar.gz \
    && tar xf "$ngx_file_name".tar.gz \
    && cp nginx/make "$ngx_file_name"/auto/ \
    && cp nginx/nginx.conf "$ngx_file_name"/conf/ \
    && chmod +x "$ngx_file_name"/configure \
    && cd "$ngx_file_name" \
    && ./configure --prefix=/etc/nginx --add-module=../../source/nginx_polaris_limit_module --add-module=../polaris_client --with-stream --with-cpp=g++ \
    && make \
    && make install \
    && ln -sf /etc/nginx/sbin/nginx /usr/local/bin/nginx \
    && ln -sf /dev/stdout /var/log/nginx/access.log \
    && ln -sf /dev/stderr /var/log/nginx/error.log

CMD ["nginx", "-g", "daemon off;", "-c", "/etc/nginx/conf/nginx.conf"]