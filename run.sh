#!/bin/bash

# 步骤 1: 创建自定义网络
if ! docker network inspect my_network >/dev/null 2>&1; then
  docker network create \
    --subnet=192.168.1.0/24 \
    my_network
fi

for container in basestation satellite1 satellite2 satellite3 satellite4 satellite5; do
  if docker ps -a --format '{{.Names}}' | grep -q "^${container}$"; then
    docker rm -f "${container}"
  fi
done

# 步骤 2: 启动基站容器
docker run -d --name basestation \
  --network my_network --ip 192.168.1.100 \
  sta-main-image

# 步骤 3: 启动第一个卫星容器
docker run -d --name satellite1 \
  --network my_network --ip 192.168.1.101 \
  satellite-image

# 步骤 4: 启动第二个卫星容器
docker run -d --name satellite2 \
  --network my_network --ip 192.168.1.102 \
  satellite-image

# 步骤 5: 启动第三个卫星容器
docker run -d --name satellite3 \
  --network my_network --ip 192.168.1.103 \
  satellite-image

# 步骤 6: 启动第四个卫星容器
docker run -d --name satellite4 \
  --network my_network --ip 192.168.1.104 \
  satellite-image

# 步骤 7: 启动第五个卫星容器
docker run -d --name satellite5 \
  --network my_network --ip 192.168.1.105 \
  satellite-image

  #docker network inspect my_network