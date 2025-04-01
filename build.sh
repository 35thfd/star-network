#!/bin/bash

# Step 1: g++ 编译命令
g++ satellite_copy.cpp -o satellite_a -pthread 
g++ station.cpp sta_main.cpp -o station -pthread 

# Step 2: Docker build 命令
# 构建 satellite-image 镜像，支持 amd64 和 arm64
docker buildx build --platform linux/amd64,linux/arm64 --network=host -t satellite-image -f Dockerfile.satellite .
# 构建 sta-main-image 镜像，支持 amd64 和 arm64
docker buildx build --platform linux/amd64,linux/arm64 --network=host -t sta-main-image -f Dockerfile.sta_main .
