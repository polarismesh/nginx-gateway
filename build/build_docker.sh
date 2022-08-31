#!/bin/bash

if [ $# != 1 ]; then
    echo "e.g.: bash $0 v1.0"
    exit 1
fi

docker_tag=$1

docker_repository="polarismesh"

echo "docker repository : ${docker_repository}/nginx, tag : ${docker_tag}"

arch_list=( "amd64" )
platforms=""

for arch in ${arch_list[@]}; do
    platforms+="linux/${arch},"
done

platforms=${platforms::-1}
extra_tags=""

pre_release=`echo ${docker_tag}|egrep "(alpha|beta|rc|[T|t]est)"|wc -l`
if [ ${pre_release} == 0 ]; then
  extra_tags="-t ${docker_repository}/nginx:latest"
fi

cd ..
docker buildx build --network=host -t ${docker_repository}/nginx:${docker_tag} ${extra_tags} --platform ${platforms} --push ./
