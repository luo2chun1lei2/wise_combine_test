#!/usr/bin/env python3
"""独立参考实现：计算连接状态机在给定长度内的路径数，用于与工具输出对照。"""
import sys
from collections import defaultdict


def connection_paths(max_len: int) -> int:
    transitions = [
        ("DISCONNECTED", "CONNECTING"),
        ("CONNECTING", "CONNECTED"),
        ("CONNECTING", "DISCONNECTED"),
        ("CONNECTED", "DISCONNECTED"),
        ("DISCONNECTED", "CLOSED"),
    ]
    adj = defaultdict(list)
    for src, dst in transitions:
        adj[src].append(dst)

    paths = []

    def dfs(state: str, path: list) -> None:
        if len(path) >= max_len:
            return
        for nxt in adj[state]:
            nxt_path = path + [(state, nxt)]
            paths.append(nxt_path)
            dfs(nxt, nxt_path)

    dfs("DISCONNECTED", [])
    return len(paths)


if __name__ == "__main__":
    print(connection_paths(int(sys.argv[1])))
