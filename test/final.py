
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
