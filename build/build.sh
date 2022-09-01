#!/bin/bash
set -e

version="debug"
if [ $# == 1 ]; then
  version=$1
fi
echo "build nginx-gateway for version ${version}"
# first make the polaris-cpp
pushd ../third_party
rm -rf polaris-cpp
git clone -b release_v1.1.0 https://github.com/polarismesh/polaris-cpp polaris-cpp

ngx_file_name=nginx-1.23.1
rm -rf "$ngx_file_name"
curl http://nginx.org/download/"$ngx_file_name".tar.gz -o "$ngx_file_name".tar.gz
tar xf "$ngx_file_name".tar.gz
cp nginx/make "$ngx_file_name"/auto/
cp nginx/nginx.conf "$ngx_file_name"/conf/

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
folder_name=$(echo "nginx-gateway-release-${version}.${build_os}.${build_arch}" | tr 'A-Z' 'a-z')
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
make install
cp ../nginx/start.sh ../../$folder_name/sbin/start.sh
cp ../nginx/stop.sh ../../$folder_name/sbin/stop.sh
popd

pushd ..
cp polaris.yaml $folder_name/conf/
tar czf "$folder_name.tar.gz" $folder_name
popd

popd