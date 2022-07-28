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

pushd nginx-1.18.0/
chmod +x configure
./configure \
    --prefix=../../$folder_name \
	--add-module=../../source/nginx_polaris_limit_module \
	--add-module=../polaris_client \
    --with-stream
make
make install
popd

pushd ..
cp polaris.yaml $folder_name/conf/
tar czf "$folder_name.tar.gz" $folder_name
popd

popd