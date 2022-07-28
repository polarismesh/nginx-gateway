#!/bin/bash
set -e

rm -rf ../target
pushd ../third_party
rm nginx-1.18.0/Makefile
rm -rf nginx-1.18.0/objs
pushd polaris-cpp
make clean
popd
popd