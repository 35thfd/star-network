#!/bin/bash

# ./control.sh <src_container> <dst_ip> <action>
# action = block（断开）| unblock（恢复）

SRC=$1
DST_IP=$2
ACTION=$3

PID=$(docker inspect -f '{{.State.Pid}}' $SRC)
NETNS_DIR="/var/run/netns"

# 创建 netns 目录和软链接
mkdir -p $NETNS_DIR
ln -sf /proc/$PID/ns/net $NETNS_DIR/$SRC

if [ "$ACTION" = "block" ]; then
  ip netns exec $SRC tc qdisc add dev eth0 root handle 1: prio
  ip netns exec $SRC tc filter add dev eth0 protocol ip parent 1:0 prio 1 u32 match ip dst $DST_IP/32 flowid 1:1
  ip netns exec $SRC tc qdisc add dev eth0 parent 1:1 handle 10: netem loss 100%
  echo "Blocked $SRC from accessing $DST_IP"
elif [ "$ACTION" = "unblock" ]; then
  ip netns exec $SRC tc qdisc del dev eth0 root
  echo "Restored $SRC access to $DST_IP"
else
  echo "Unknown action: $ACTION"
  exit 1
fi
