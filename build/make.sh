#!/bin/bash

set -e

ngx_file_name=nginx-1.23.1

pushd ../third_party
rm -rf polaris-cpp
git clone -b release_v1.1.0 https://github.com/polarismesh/polaris-cpp polaris-cpp
rm -rf "$ngx_file_name"
curl http://nginx.org/download/"$ngx_file_name".tar.gz -o "$ngx_file_name".tar.gz
tar xf "$ngx_file_name".tar.gz
cp nginx/make "$ngx_file_name"/auto/

pushd polaris-cpp
make
make package
# copy libraries and headers
tar xf polaris_cpp_sdk.tar.gz
popd

mv polaris-cpp/polaris_cpp_sdk/* polaris_client/
# build nginx
build_os=$(uname -s)
build_arch=$(uname -p)
folder_name="target"
echo "target name $folder_name"
rm -rf ../$folder_name
mkdir -p ../$folder_name

pushd "$ngx_file_name"/
chmod +x configure
./configure \
    --prefix=../../$folder_name \
	--add-module=../../source/nginx_polaris_limit_module \
	--add-module=../polaris_client \
    --with-stream \
    --with-cpp=g++
make
popd

popd