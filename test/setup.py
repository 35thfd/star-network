import random
from collections import defaultdict, deque

N = 100  # 卫星数量
F = 10   # 分片数量
K = 6    # 每个卫星的邻居上限

random.seed(42)  # 保证可复现性

# 构建邻接图（mesh 网络，每个节点最多 K 个邻居）
def build_network():
    neighbors = defaultdict(set)
    for i in range(N):
        while len(neighbors[i]) < K:
            j = random.randint(0, N - 1)
            if j != i and len(neighbors[j]) < K:
                neighbors[i].add(j)
                neighbors[j].add(i)
    return neighbors

