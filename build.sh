#!/bin/bash

# Step 2: Docker build 命令
# 构建 satellite-image 镜像，支持 amd64 和 arm64


# docker build -t satellite-image -f Dockerfile.satellite .
# docker build -t sta-main-image -f Dockerfile.sta_main .

docker rm -f satelliteax
docker rm -f satellitebx
docker rm -f satellitecx
docker rm -f sta_mainx

docker run -d --name satelliteax --network my_network satellite-image
docker run -d --name satellitebx --network my_network satellite-image
docker run -d --name satellitecx --network my_network satellite-image
docker run -d --name sta_mainx --network my_network sta-main-image

# containers=("sta_main" "satellitea" "satelliteb","satellitec")

# for container in "${containers[@]}"; do
#     code --new-terminal -- bash -c "echo '进入容器: $container'; docker exec -it $container /bin/bash; exec bash"
# done
# osascript -e 'tell application "Terminal" to do script "docker exec -it sta_main /bin/bash -c \"./station\""'
# osascript -e 'tell application "Terminal" to do script "docker exec -it satellitea /bin/bash -c \"./satellite\""'
# osascript -e 'tell application "Terminal" to do script "docker exec -it satelliteb /bin/bash -c \"./satellite\""'
# osascript -e 'tell application "Terminal" to do script "docker exec -it satellitec /bin/bash -c \"./satellite\""'

# docker exec -it satelliteax /bin/bash
# docker exec -it satellitebx /bin/bash
# docker exec -it satellitecx /bin/bash
# docker exec -it sta_mainx /bin/bash