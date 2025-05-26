# #!/bin/bash

# ACTION=$1       # block/unblock
# TARGET_IP=$2    # 目标 IP

# if [ "$ACTION" == "block" ]; then
#     echo "Blocking $TARGET_IP"
#     tc qdisc add dev eth0 root handle 1: prio
#     tc filter add dev eth0 protocol ip parent 1:0 prio 1 u32 match ip dst $TARGET_IP flowid :2
#     tc qdisc add dev eth0 parent 1:2 handle 20: netem loss 100%
# elif [ "$ACTION" == "unblock" ]; then
#     echo "Unblocking $TARGET_IP"
#     tc qdisc del dev eth0 root
# else
#     echo "Usage: control_tc.sh block|unblock <target_ip>"
# fi

#!/bin/bash

ACTION=$1
TARGET_IP=$2

if [ -z "$ACTION" ] || [ -z "$TARGET_IP" ]; then
    echo "Usage: $0 block|unblock <target_ip>"
    exit 1
fi

# 检查是否已有 qdisc
ROOT_EXISTS=$(tc qdisc show dev eth0 | grep "handle 1:")
if [ "$ACTION" == "block" ]; then
    echo "Blocking $TARGET_IP"
    
    if [ -z "$ROOT_EXISTS" ]; then
        # 添加 root qdisc（优先级调度器）
        tc qdisc add dev eth0 root handle 1: prio
    fi

    # 检查是否已经存在目标规则，避免重复添加失败
    tc filter show dev eth0 parent 1:0 | grep "$TARGET_IP" >/dev/null
    if [ $? -ne 0 ]; then
        # 添加 IP 匹配规则（将目标 IP 流量引导到子队列 2）
        tc filter add dev eth0 protocol ip parent 1:0 prio 1 u32 match ip dst $TARGET_IP flowid 1:2
        # 给子队列添加 100% 丢包模拟
        tc qdisc add dev eth0 parent 1:2 handle 20: netem loss 100%
    else
        echo "Rule for $TARGET_IP already exists."
    fi

elif [ "$ACTION" == "unblock" ]; then
    echo "Unblocking $TARGET_IP"

    # 移除全部 qdisc（简单粗暴）
    tc qdisc del dev eth0 root

else
    echo "Invalid action: $ACTION"
    exit 1
fi
