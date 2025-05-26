#!/bin/bash

# docker build -t satellite-image -f Dockerfile.satellite .
# docker build -t sta-main-image -f Dockerfile.sta_main .

NUM_SATS=10
NETWORK_NAME=my_network
# IMAGE_NAME=satellite-image
BLOCK_INTERVAL=10  # 每隔多少秒模拟拓扑变化

docker rm -f station >/dev/null 2>&1
for i in $(seq 1 $NUM_SATS); do
    docker rm -f sat$i >/dev/null 2>&1
done

docker network inspect $NETWORK_NAME >/dev/null 2>&1 || \
    docker network create $NETWORK_NAME

echo "启动 station 容器"
docker run -d --name station --network $NETWORK_NAME --cap-add=NET_ADMIN sta-main-image
osascript -e 'tell application "Terminal" to do script "docker exec -it station /bin/bash -c \"./station\""'


echo "🛰️启动 $NUM_SATS 个卫星容器..."
for i in $(seq 1 $NUM_SATS); do
    docker run -d --name sat$i --network $NETWORK_NAME --cap-add=NET_ADMIN satellite-image
    osascript -e 'tell application "Terminal" to do script "docker exec -it sat'"$i"' /bin/bash -c \"./satellite\""' 
done

echo "📡获取容器IP映射..."
declare -A ip_map
for name in station $(seq -f "sat%g" 1 $NUM_SATS); do
    ip=$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' $name)
    ip_map[$name]=$ip
done

echo "所有容器和 IP 地址："
for name in "${!ip_map[@]}"; do
    echo "$name => ${ip_map[$name]}"
done

echo "开始动态模拟连接通断..."
while true; do
    echo "=== Simulating random connectivity ==="

    # 构建所有可能的通信对
    all=()
    for src in "${!ip_map[@]}"; do
        for dst in "${!ip_map[@]}"; do
            [ "$src" != "$dst" ] && all+=("$src,$dst")
        done
    done

    # 随机选几对 block/unblock
    for i in $(seq 1 10); do
        idx=$((RANDOM % ${#all[@]}))
        pair=${all[$idx]}
        IFS=, read -r src dst <<< "$pair"
        ip_dst=${ip_map[$dst]}
        op=$((RANDOM % 2))

        if [ $op -eq 0 ]; then
            echo "⛔Blocking $src -> $dst"
            docker exec $src bash ./control_tc.sh block $ip_dst
        else
            echo "✅Unblocking $src -> $dst"
            docker exec $src bash ./control_tc.sh unblock $ip_dst
        fi
    done

    sleep $BLOCK_INTERVAL
done