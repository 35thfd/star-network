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
IFACE="eth0" # Docker 容器内通常是 eth0

if [ -z "$ACTION" ] || [ -z "$TARGET_IP" ]; then
    echo "Usage: $0 block|unblock <target_ip>"
    exit 1
fi

# 函数：确保根 qdisc 为 prio 1:
ensure_prio_qdisc() {
    # 检查 eth0 root 是否已经有一个 handle 1: 的 prio qdisc
    if tc qdisc show dev $IFACE | grep -q "qdisc prio 1:"; then
        # echo "DEBUG: qdisc prio 1: already exists on $IFACE."
        return 0
    else
        echo "INFO: Root qdisc 'prio 1:' not found. Attempting to set/replace it on $IFACE."
        # 使用 replace 替换或添加根 qdisc
        tc qdisc replace dev $IFACE root handle 1: prio bands 3 priomap 1 2 2 2 1 2 0 0 1 1 1 1 1 1 1 1
        if [ $? -eq 0 ]; then
            echo "INFO: Successfully set/replaced root qdisc with prio 1: on $IFACE."
            return 0
        else
            echo "ERROR: Failed to set/replace root qdisc with prio 1: on $IFACE using 'replace'."
            # 尝试 'add' 作为备选
            tc qdisc add dev $IFACE root handle 1: prio bands 3 priomap 1 2 2 2 1 2 0 0 1 1 1 1 1 1 1 1
            if [ $? -eq 0 ]; then
                echo "INFO: Successfully added root qdisc with prio 1: on $IFACE using 'add'."
                return 0
            else
                echo "ERROR: Also failed to add root qdisc with prio 1: on $IFACE using 'add'."
                return 1 # 指示失败
            fi
        fi
    fi
}

if [ "$ACTION" == "block" ]; then
    echo "Blocking $TARGET_IP on $IFACE"
    
    ensure_prio_qdisc
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to initialize root qdisc for $TARGET_IP. Aborting block."
        exit 1
    fi

    FILTER_BLOCK_PRIO=100 # 为阻塞规则定义一个 tc filter 优先级

    # 尝试删除任何可能已存在的针对此 IP 和优先级的旧 filter，以确保幂等性
    # 忽略错误，因为 filter 可能本就不存在
    tc filter del dev $IFACE parent 1:0 protocol ip prio $FILTER_BLOCK_PRIO u32 match ip dst $TARGET_IP > /dev/null 2>&1

    # 添加 filter 将目标 IP 的流量直接丢弃，使用 'action drop'
    echo "INFO: Adding filter for $TARGET_IP with priority $FILTER_BLOCK_PRIO using 'action drop'"
    tc filter add dev $IFACE parent 1:0 protocol ip prio $FILTER_BLOCK_PRIO u32 match ip dst $TARGET_IP action drop
    #                                                                                                     ^^^^^^^^^^^ 主要修改点
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to add drop filter for $TARGET_IP."
        echo "DEBUG: Current filters on $IFACE parent 1:0 are:"
        tc filter show dev $IFACE parent 1:0
    else
        echo "INFO: Successfully added drop filter for $TARGET_IP."
    fi

elif [ "$ACTION" == "unblock" ]; then
    echo "Unblocking $TARGET_IP on $IFACE"

    if tc qdisc show dev $IFACE | grep -q "qdisc prio 1:"; then
        FILTER_BLOCK_PRIO=100 # 必须与 'block' 时使用的优先级一致
        echo "INFO: Deleting filter for $TARGET_IP with priority $FILTER_BLOCK_PRIO"
        tc filter del dev $IFACE parent 1:0 protocol ip prio $FILTER_BLOCK_PRIO u32 match ip dst $TARGET_IP
        if [ $? -ne 0 ]; then
            echo "WARN: Failed to delete filter for $TARGET_IP (it might not have existed, or tc parameters for deletion were not precise enough)."
            echo "DEBUG: Current filters on $IFACE parent 1:0 are:"
            tc filter show dev $IFACE parent 1:0
        else
            echo "INFO: Successfully deleted filter for $TARGET_IP."
        fi
    else
        echo "WARN: Root prio qdisc (handle 1:) not found on $IFACE. Cannot remove specific filter for $TARGET_IP."
        echo "INFO: Interface $IFACE might already be in a default state or has a different qdisc setup."
    fi
else
    echo "Invalid action: $ACTION. Use 'block' or 'unblock'."
    exit 1
fi