#!/bin/bash

# 参数解析
SRC_CONTAINER=$1
TARGET_IP=$2
ACTION=$3

if [ -z "$SRC_CONTAINER" ] || [ -z "$TARGET_IP" ] || [ -z "$ACTION" ]; then
    echo "Usage: $0 <source_container> <target_ip> <block|unblock>"
    exit 1
fi

# 获取源容器的网络命名空间 PID
SRC_PID=$(docker inspect -f '{{.State.Pid}}' "$SRC_CONTAINER")
if [ -z "$SRC_PID" ]; then
    echo "Failed to get PID for container $SRC_CONTAINER"
    exit 1
fi

# 定义 network namespace 路径
NETNS_PATH="/proc/$SRC_PID/ns/net"

# 设置 tc 命令（注意必须指定容器内的 eth0）
if [ "$ACTION" == "block" ]; then
    echo "Blocking $TARGET_IP from $SRC_CONTAINER"
    nsenter --net=$NETNS_PATH tc qdisc add dev eth0 root handle 1: prio
    nsenter --net=$NETNS_PATH tc filter add dev eth0 protocol ip parent 1:0 prio 1 u32 match ip dst $TARGET_IP flowid :2
    nsenter --net=$NETNS_PATH tc qdisc add dev eth0 parent 1:2 handle 20: netem loss 100%
elif [ "$ACTION" == "unblock" ]; then
    echo "Unblocking $TARGET_IP from $SRC_CONTAINER"
    nsenter --net=$NETNS_PATH tc qdisc del dev eth0 root
else
    echo "Invalid action: $ACTION (must be block or unblock)"
    exit 1
fi
