#!/opt/homebrew/bin/bash

docker build -t satellite-image -f Dockerfile.satellite .
docker build -t sta-main-image -f Dockerfile.sta_main .

NUM_SATS=3
NETWORK_NAME=my_network
BLOCK_INTERVAL=3  # 每隔多少秒模拟拓扑变化

# 清理旧容器
docker rm -f station >/dev/null 2>&1
for i in $(seq 1 $NUM_SATS); do
    docker rm -f sat$i >/dev/null 2>&1
done

# 创建网络
docker network inspect $NETWORK_NAME >/dev/null 2>&1 || \
    docker network create $NETWORK_NAME

# 启动 station
echo "启动 station 容器"
docker run -dit --name station --network $NETWORK_NAME --cap-add=NET_ADMIN sta-main-image ./station

# 启动 satellites
echo "🛰️ 启动 $NUM_SATS 个卫星容器..."
for i in $(seq 1 $NUM_SATS); do
    docker run -dit --name sat$i --network $NETWORK_NAME --cap-add=NET_ADMIN satellite-image ./satellite
done

# 获取容器 IP 映射
echo "📡 获取容器IP映射..."
declare -A ip_map
for name in station $(seq -f "sat%g" 1 $NUM_SATS); do
    ip=$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' $name)
    ip_map[$name]=$ip
done

echo "所有容器和 IP 地址："
for name in "${!ip_map[@]}"; do
    echo "$name => ${ip_map[$name]}"
done

# 实时日志聚合
echo "📜 开始实时聚合日志..."
for name in station $(seq -f "sat%g" 1 $NUM_SATS); do
    docker logs -f "$name" 2>&1 | sed "s/^/[$name] /" &
done

# 动态模拟连接通断
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
            echo "⛔ Blocking $src -> $dst"
            docker exec $src bash ./control_tc.sh block $ip_dst
        else
            echo "✅ Unblocking $src -> $dst"
            docker exec $src bash ./control_tc.sh unblock $ip_dst
        fi
    done

    sleep $BLOCK_INTERVAL
done
