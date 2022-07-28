#!/bin/bash

set -e

pushd ../third_party
rm -rf polaris-cpp
git clone -b release_v1.1.0 https://github.com/polarismesh/polaris-cpp polaris-cpp

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

pushd nginx-1.18.0/
chmod +x configure
./configure \
    --prefix=../../$folder_name \
	--add-module=../../source/nginx_polaris_limit_module \
	--add-module=../polaris_client \
    --with-stream
make
popd

popd