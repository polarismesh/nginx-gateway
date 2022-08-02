#!/bin/bash

curpath=$(pwd)

if [ "${0:0:1}" == "/" ]; then
    dir=$(dirname "$0")
else
    dir=$(pwd)/$(dirname "$0")
fi

cd $dir

function log_date() {
    echo $(date "+%Y-%m-%dT%H:%M:%S")
}

function log_info() {
    echo -e "\033[32m\033[01m$(log_date)\tinfo\t$1 \033[0m"
}

pids=$(ps -ef | grep -w "nginx: master" | grep -v "grep" | awk '{print $2}')
array=($pids)
for pid in ${array[@]}; do
   log_info "stop nginx: pid=$pid"
   kill -15 $pid
done

#------------------------------------------------------

cd $curpath
