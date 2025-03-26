#!/bin/bash
make clean

IMAGE_NAME="star_network"
STA_CONTAINER="station_container"
SAT_CONTAINER_PREFIX="satellite_container"

echo "停止并删除地面站容器..."
docker stop $STA_CONTAINER 2>/dev/null
docker rm $STA_CONTAINER 2>/dev/null

echo "停止并删除卫星容器..."
for i in $(seq 1 3); do
    CONTAINER_NAME="${SAT_CONTAINER_PREFIX}_${i}"
    docker stop $CONTAINER_NAME 2>/dev/null
    docker rm $CONTAINER_NAME 2>/dev/null
done

echo "删除 Docker 镜像..."
docker rmi $IMAGE_NAME 2>/dev/null

echo "所有相关 Docker 资源已清理完毕！"
