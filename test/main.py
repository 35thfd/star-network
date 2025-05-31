import random
from collections import defaultdict, deque

N = 300  # 卫星数量
F = 10   # 分片数量
K = 6    # 每个卫星的邻居上限

random.seed(42)  # 保证可复现性

def build_network():
    neighbors = defaultdict(set)
    for i in range(N):
        while len(neighbors[i]) < K:
            j = random.randint(0, N - 1)
            if j != i and len(neighbors[j]) < K:
                neighbors[i].add(j)
                neighbors[j].add(i)
    return neighbors

def flooding(neighbors):
    received = [set() for _ in range(N)]
    send_count = 0
    queue = deque([(i, f) for f in range(F) for i in neighbors[0]])  # 基站广播给邻居

    for f in range(F):
        received[0].add(f)  # 基站自带所有分片

    while queue:
        node, frag = queue.popleft()
        if frag in received[node]:
            continue
        received[node].add(frag)
        send_count += len(neighbors[node])
        for nb in neighbors[node]:
            queue.append((nb, frag))
    return send_count

def selective_retransmission(neighbors):
    received = [set() for _ in range(N)]
    send_count = 0

    # 初始广播（基站发给邻居）
    for nb in neighbors[0]:
        for f in range(F):
            received[nb].add(f)
            send_count += 1
    received[0] = set(range(F))

    changed = True
    while changed:
        changed = False
        for i in range(1, N):
            missing = set(range(F)) - received[i]
            if not missing:
                continue
            for f in missing:
                for nb in neighbors[i]:
                    if f in received[nb]:
                        received[i].add(f)
                        send_count += 1
                        changed = True
                        break
    return send_count

def cooperative_forwarding(neighbors):
    received = [set() for _ in range(N)]
    send_count = 0

    for nb in neighbors[0]:
        for f in range(F):
            received[nb].add(f)
            send_count += 1
    received[0] = set(range(F))

    changed = True
    while changed:
        changed = False
        for i in range(1, N):
            missing = set(range(F)) - received[i]
            if not missing:
                continue
            for f in missing:
                helper_found = False
                for nb in neighbors[i]:
                    if f in received[nb]:
                        received[i].add(f)
                        send_count += 1
                        changed = True
                        helper_found = True
                        break
                if helper_found:
                    continue
    return send_count


def hybrid_strategy(neighbors):
    received = [set() for _ in range(N)]
    send_count = 0
    received[0] = set(range(F))

    # 初始广播（基站只发给一圈）
    for nb in neighbors[0]:
        for f in range(F):
            received[nb].add(f)
            send_count += 1

    changed = True
    while changed:
        changed = False
        for i in range(1, N):
            missing = set(range(F)) - received[i]
            if not missing:
                continue
            needed = list(missing)
            random.shuffle(needed)
            for f in needed:
                providers = [nb for nb in neighbors[i] if f in received[nb]]
                if providers:
                    sender = random.choice(providers)
                    received[i].add(f)
                    send_count += 1
                    changed = True
    return send_count

def selective_retransmission_v2(neighbors):
    received = [set() for _ in range(N)]
    send_count = 0
    timeout_count = [defaultdict(int) for _ in range(N)]

    for nb in neighbors[0]:
        for f in range(F):
            received[nb].add(f)
            send_count += 1
    received[0] = set(range(F))

    for round in range(20):  # 限制重传轮数
        for i in range(1, N):
            missing = set(range(F)) - received[i]
            for f in missing:
                timeout_count[i][f] += 1
                if timeout_count[i][f] >= 2:  # 模拟超时两轮才请求
                    for nb in neighbors[i]:
                        send_count += 1  # 无论是否成功都计入一次尝试
                        if f in received[nb]:
                            received[i].add(f)
                            break
        if all(len(received[i]) == F for i in range(N)):
            break
    return send_count

def cooperative_forwarding_v2(neighbors):
    received = [set() for _ in range(N)]
    send_count = 0

    for nb in neighbors[0]:
        for f in range(F):
            received[nb].add(f)
            send_count += 1
    received[0] = set(range(F))

    for round in range(15):  # 模拟轮次传播
        for i in range(N):
            for f in received[i]:
                for nb in neighbors[i]:
                    send_count += 1
                    if f not in received[nb]:
                        received[nb].add(f)
    return send_count


def run_all():
    neighbors = build_network()
    print("Flooding:", flooding(neighbors))
    print("Selective Retransmission:", selective_retransmission_v2(neighbors))
    print("Cooperative Forwarding:", cooperative_forwarding_v2(neighbors))
    print("Hybrid (实验组):", hybrid_strategy(neighbors))

run_all()

def main():
    run_all()