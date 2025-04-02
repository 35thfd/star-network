#!/bin/bash

# 创建 Docker 网络（如果不存在）
NETWORK_NAME="satellite_network"
docker network ls | grep -q "$NETWORK_NAME" || docker network create "$NETWORK_NAME"

docker build -t satellite_image -f Dockerfile.satellite .
docker build -t sta_main_image -f Dockerfile.sta_main .


# 运行 satellite 容器
docker run -d --name satellite --network "$NETWORK_NAME" satellite_image

# 运行 sta_main 容器
docker run -d --name sta_main --network "$NETWORK_NAME" sta_main_image

echo "🚀 容器已启动"
