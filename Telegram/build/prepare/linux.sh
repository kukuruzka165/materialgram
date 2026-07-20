#!/bin/bash

set -e
FullExecPath=$PWD
pushd `dirname $0` > /dev/null
FullScriptPath=`pwd`
popd > /dev/null


cd $FullScriptPath/../docker/centos_env
poetry install
poetry run gen_dockerfile | DOCKER_BUILDKIT=1 docker build -t materialgram:centos_env -

# VS Code Dev Containers caches its own derived "vsc-materialgram-*-uid" image
# the first time it builds the container and never notices that the
# materialgram:centos_env tag above moved. Drop the cached copy so the next
# "Reopen in Container" is forced to rebuild from the image we just produced.
for img in $(docker images -q --filter "reference=vsc-materialgram-*"); do
  containers=$(docker ps -aq --filter "ancestor=$img")
  if [ -n "$containers" ]; then
    docker rm -f $containers > /dev/null || true
  fi
  docker rmi "$img" > /dev/null || true
done

cd $FullExecPath
