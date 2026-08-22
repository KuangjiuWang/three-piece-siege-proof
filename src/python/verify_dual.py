# -*- coding: utf-8 -*-
"""双色规则书的独立终验(不查胜负表, 两个角色分别独立穷举):

RUN 1 (黑角色): 种子 {(INIT,黑先), (INIT,白先)}; 黑按书走, 白穷举一切合法动作。
   断言: 书总给出合法走法; 白无立即获胜动作; 黑从不困毙。
   => 可达集 Sigma 是"黑方安全集", 且含 (INIT,0) 与 (INIT,1)。

RUN 2 (白角色): 种子 {(INIT,黑先)}; 白按 rho 共轭的同一本书走, 黑穷举一切合法动作。
   断言: 书总给出合法走法; 黑无立即获胜动作; 白从不困毙。
   => 可达集是"白方安全集", 且含 (INIT,0)。

两者合并: 初始局面双方都有不败策略 => 平局(且任何不以某方获胜结束的对局
必在有限步内三次重复同一完整局面而判和)。
"""
from collections import deque, Counter
from engine import *
from dualbook import (black_move_ex, white_move_ex, apply_black, apply_white,
                      rho, BOOK)

INIT0 = (0xF000, 0x000F, 0)
INIT1 = (0xF000, 0x000F, 1)
VAL = open("values.bin", "rb").read()

def run(seeds, me):
    """me = 0: 黑按书走, 白穷举;  me = 1: 白按书走, 黑穷举。"""
    seen = set(seeds); q = deque(seeds)
    n_me = n_opp = t_win = t_stuck = 0
    val_bad = 0
    mats = Counter()
    while q:
        st = q.popleft()
        B, W, t = st
        if t == me:
            n_me += 1
            mats[(popcount(B), popcount(W))] += 1
            if me == 0:
                mv, why = black_move_ex(st); r = apply_black(st, mv) if mv else None
            else:
                mv, why = white_move_ex(st); r = apply_white(st, mv) if mv else None
            assert mv is not None, ("书未给出走法", st, why)
            assert r is not None, ("书给出非法走法", st, mv, why)
            if r == ('win',):
                t_win += 1; continue
            ns = r[1]
            # 独立参考: 全空间胜负表不应判定"对手必胜"
            if VAL[sidx(ns)] == 1: val_bad += 1
            if ns not in seen: seen.add(ns); q.append(ns)
        else:
            n_opp += 1
            acts = successors(st)
            if not acts:
                t_stuck += 1; continue          # 对手困毙 => 我方获胜(终局)
            for (x, y, z, res) in acts:
                assert res != ('win',), ("!!对手存在立即获胜动作", st, (x, y, z))
                ns = res[1]
                assert popcount(ns[0]) >= 3 and popcount(ns[1]) >= 3
                if ns not in seen: seen.add(ns); q.append(ns)
    return dict(closure=len(seen), n_me=n_me, n_opp=n_opp, win=t_win,
                stuck=t_stuck, val_bad=val_bad, mats=dict(sorted(mats.items())),
                seen=seen)

print("=" * 68)
print("RUN 1  黑方角色: 黑按书走, 白穷举一切走法, 种子 = {(INIT,黑先), (INIT,白先)}")
r1 = run([INIT0, INIT1], 0)
for k in ("closure", "n_me", "n_opp", "win", "stuck", "val_bad"):
    print("   %-10s %d" % (k, r1[k]))
print("   材料分布(黑,白):", r1["mats"])

print("=" * 68)
print("RUN 2  白方角色: 白按 rho(书) 走, 黑穷举一切走法, 种子 = {(INIT,黑先)}")
r2 = run([INIT0], 1)
for k in ("closure", "n_me", "n_opp", "win", "stuck", "val_bad"):
    print("   %-10s %d" % (k, r2[k]))
print("   材料分布(黑,白):", r2["mats"])

# 结构性检查: RUN2 的可达集应恰为 RUN1 可达集在 rho 下的像的子集
img = {rho(s) for s in r1["seen"]}
inside = all(s in img for s in r2["seen"])
print("=" * 68)
print("RUN2 reachable set is a subset of rho(RUN1 reachable set):", inside)
print("|Sigma| = %d,  |rho(Sigma)| = %d,  |RUN2 可达集| = %d"
      % (len(r1["seen"]), len(img), len(r2["seen"])))
print("(INIT,黑先) ∈ Sigma :", INIT0 in r1["seen"], "   (INIT,白先) ∈ Sigma :", INIT1 in r1["seen"])
n_exc = sum(1 for it in BOOK.values() for x in it if x[0] == 'cond')
n_ord = sum(1 for it in BOOK.values() for x in it if x[0] == 'mv')
print("书规模: 形状条目=%d, 有序候选=%d, 条件规则=%d" % (len(BOOK), n_ord, n_exc))
assert r1["val_bad"] == 0 and r2["val_bad"] == 0
print("\nALL CHECKS PASSED  ==> 同一本书对两方都是不败策略, 初始局面为平局")
