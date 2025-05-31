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
