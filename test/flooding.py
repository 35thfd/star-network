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
