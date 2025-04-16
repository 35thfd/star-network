#!/bin/bash

# Step 2: Docker build 命令
# 构建 satellite-image 镜像，支持 amd64 和 arm64


docker build -t satellite-image -f Dockerfile.satellite .
docker build -t sta-main-image -f Dockerfile.sta_main .

docker rm -f satelliteaa
docker rm -f satellitebb
docker rm -f satellitecc
docker rm -f sta_main3

docker run -d --name satelliteaa --network my_network satellite-image
docker run -d --name satellitebb --network my_network satellite-image
docker run -d --name satellitecc --network my_network satellite-image
docker run -d --name sta_main3 --network my_network sta-main-image

# containers=("sta_main" "satellitea" "satelliteb","satellitec")

# for container in "${containers[@]}"; do
#     code --new-terminal -- bash -c "echo '进入容器: $container'; docker exec -it $container /bin/bash; exec bash"
# done
# osascript -e 'tell application "Terminal" to do script "docker exec -it sta_main3 /bin/bash -c \"./station\""'
# osascript -e 'tell application "Terminal" to do script "docker exec -it satelliteaa /bin/bash -c \"./satellite\""'
# osascript -e 'tell application "Terminal" to do script "docker exec -it satellitebb /bin/bash -c \"./satellite\""'
# osascript -e 'tell application "Terminal" to do script "docker exec -it satellitecc /bin/bash -c \"./satellite\""'

# powershell.exe -Command "wt new-tab --title 'sta_main' wsl -d Ubuntu-24.04 -- bash -c 'docker exec -it sta_main /bin/bash -c \"./station\"; exec bash' ; new-tab --title 'satellitea' wsl -d Ubuntu-24.04 -- bash -c 'docker exec -it satellitea /bin/bash -c \"./satellite\"; exec bash' ; new-tab --title 'satelliteb' wsl -d Ubuntu-24.04 -- bash -c 'docker exec -it satelliteb /bin/bash -c \"./satellite\"; exec bash' ; new-tab --title 'satellitec' wsl -d Ubuntu-24.04 -- bash -c 'docker exec -it satellitec /bin/bash -c \"./satellite\"; exec bash'"
# docker exec -it satelliteaa /bin/bash
# docker exec -it satellitebb /bin/bash
# docker exec -it satellitecc /bin/bash
# docker exec -it sta_main3 /bin/bash