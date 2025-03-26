#!/bin/bash

IMAGE_NAME="star_network"
STA_CONTAINER="station"
SAT_CONTAINER_PREFIX="satellite"

#如果镜像已经存在，就不构建
if ! docker images | grep -q "$IMAGE_NAME"; then
    echo "构建Docker镜像: $IMAGE_NAME ..."
    docker build -t $IMAGE_NAME .
else
    echo "已构建: $IMAGE_NAME"
fi

# 启动sta_main
echo "启动地面站Docker"
docker stop $STA_CONTAINER 2>/dev/null
docker rm $STA_CONTAINER 2>/dev/null
docker run -dit --name $STA_CONTAINER $IMAGE_NAME ./sta_main

# 启动satellite
for i in $(seq 1 3); do
    CONTAINER_NAME="${SAT_CONTAINER_PREFIX}_${i}"
    echo "启动卫星 $CONTAINER_NAME ..."
    docker stop $CONTAINER_NAME 2>/dev/null
    docker rm $CONTAINER_NAME 2>/dev/null
    docker run -it --name $CONTAINER_NAME $IMAGE_NAME ./satellite
done

echo "全部启动成功"