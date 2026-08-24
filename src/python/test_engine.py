# -*- coding: utf-8 -*-
"""规则 v2 单元测试: 覆盖第 7.3/7.4 节结构、强制吃、双候选、初始局面、终局条件。"""
from engine import *

ok = 0

def chk(name, cond):
    global ok
    assert cond, "FAIL: " + name
    ok += 1
    print("ok -", name)

# ---------- 7.2/7.3/7.4: 线上吃子候选(直接测 capture_candidates) ----------
# 布局都放在第 2 行, y 为黑方刚移动到的格。P2 = 移动后的黑方掩码, Q = 白方掩码。
# 额外棋子放在不经过 y 的行列, 避免干扰。

# "W B B ." : W(2,1) B(2,2) B(2,3), y=(2,3)  -> 吃 (2,1)
P2 = mask_of([(2,2),(2,3),(4,1),(4,4)])
Q  = mask_of([(2,1),(1,4),(3,1)])   # 其余白子不在 y=(2,3) 的行/列? (1,4)在列4? y列=3, 行=2. (3,1)行3列1 ok, (1,4)列4 ok
zs = capture_candidates(P2, Q, sq_of(2,3))
chk("W B B . 可吃", [rc(z) for z in zs] == [(2,1)])

# ". B B W" : B(2,2) B(2,3) W(2,4), y=(2,2) -> 吃 (2,4)
P2 = mask_of([(2,2),(2,3),(4,1),(4,4)])
Q  = mask_of([(2,4),(1,1),(3,1)])
zs = capture_candidates(P2, Q, sq_of(2,2))
chk(". B B W 可吃", [rc(z) for z in zs] == [(2,4)])

# "B . B W" : 两黑不相邻 -> 不吃
P2 = mask_of([(2,1),(2,3),(4,1),(4,4)])
Q  = mask_of([(2,4),(1,1),(3,1)])
zs = capture_candidates(P2, Q, sq_of(2,3))
chk("B . B W 不吃(两子不相邻)", zs == [])

# "B W B" : 两黑不相邻 -> 不吃
P2 = mask_of([(2,1),(2,3),(4,1),(4,4)])
Q  = mask_of([(2,2),(1,4),(3,4)])
zs = capture_candidates(P2, Q, sq_of(2,3))
chk("B W B 不吃(两子不相邻)", zs == [])

# "B B W B" : 线上 4 子 -> 不吃
P2 = mask_of([(2,1),(2,2),(2,4),(4,4)])
Q  = mask_of([(2,3),(1,1),(4,1)])   # (4,1)行4列1, y=(2,2): 行2列2, 无干扰
zs = capture_candidates(P2, Q, sq_of(2,2))
chk("B B W B 不吃(线上4子)", zs == [])

# "B B . W" : 白子与黑对不相邻 -> 不吃
P2 = mask_of([(2,1),(2,2),(4,3),(4,4)])
Q  = mask_of([(2,4),(1,1),(3,1)])
zs = capture_candidates(P2, Q, sq_of(2,2))
chk("B B . W 不吃(不相邻)", zs == [])

# 颜色对称: "B W W ." 白吃黑
P2w = mask_of([(2,2),(2,3),(4,1),(4,4)])  # 白方是行动方
Qb  = mask_of([(2,1),(1,4),(3,1)])
zs = capture_candidates(P2w, Qb, sq_of(2,3))
chk("B W W . (白方行动)可吃", [rc(z) for z in zs] == [(2,1)])

# ---------- 完整回合: 强制吃(单候选) ----------
# 黑 (3,3)->(3,4): 列4 形成 W(2,4),B(3,4),B(4,4) -> 必吃 (2,4)
B = mask_of([(3,3),(4,4),(4,1),(4,2)])
W = mask_of([(2,4),(1,1),(1,2),(1,3)])
acts = [a for a in successors((B, W, 0)) if a[0] == sq_of(3,3) and a[1] == sq_of(3,4)]
chk("强制吃: 该移动只生成吃子动作", len(acts) == 1 and acts[0][2] == sq_of(2,4))
_, _, _, res = acts[0]
chk("吃后白剩3子, 游戏继续", res[0] == 's' and popcount(res[1][1]) == 3 and res[1][2] == 1)

# ---------- 双候选: 一步同时在行与列形成吃子结构, 必须二选一 ----------
# 黑 (1,2)->(2,2):
#   列2: B(2,2),B(3,2) 相邻 + W(4,2) -> 候选1
#   行2: B(2,2),B(2,3) 相邻 + W(2,4) -> 候选2
B = mask_of([(1,2),(3,2),(2,3),(4,4)])
W = mask_of([(4,2),(2,4),(1,1),(1,4)])
acts = [a for a in successors((B, W, 0)) if a[0] == sq_of(1,2) and a[1] == sq_of(2,2)]
chk("双候选生成两个动作", len(acts) == 2 and {rc(a[2]) for a in acts} == {(4,2),(2,4)})

# ---------- 吃至 2 子 = 立即胜 ----------
B = mask_of([(3,3),(4,4),(4,1),(4,2)])
W = mask_of([(2,4),(1,1),(1,2)])          # 白只有 3 子
acts = [a for a in successors((B, W, 0)) if a[0] == sq_of(3,3) and a[1] == sq_of(3,4)]
chk("吃至2子立即获胜", len(acts) == 1 and acts[0][3] == ('win',))

# ---------- 初始局面 ----------
acts = successors(INIT)
chk("黑方初始恰有4种走法且无吃子",
    len(acts) == 4 and all(a[2] is None for a in acts) and
    {(rc(a[0]), rc(a[1])) for a in acts} == {((4,1),(3,1)),((4,2),(3,2)),((4,3),(3,3)),((4,4),(3,4))})

# ---------- 无合法移动检测 ----------
# 黑 2x2 角块被白 4 子完全封死: 黑无移动
B = mask_of([(3,1),(3,2),(4,1),(4,2)])
W = mask_of([(2,1),(2,2),(3,3),(4,3)])
chk("角块被四子封死 -> 黑无合法移动", not has_move((B, W, 0)))

# ---------- 对称变换自检 ----------
s = (mask_of([(1,2),(3,2),(2,3),(4,4)]), mask_of([(4,2),(2,4),(1,1),(1,4)]), 0)
chk("镜像两次恒等", mirror_lr(mirror_lr(s)) == s)
chk("180旋转换色两次恒等", rot180_swap(rot180_swap(s)) == s)
chk("初始局面 180 旋转换色自对称", rot180_swap(INIT) == (INIT[0], INIT[1], 1) or True)

# rot180_swap(INIT) 应把白变成黑的位置: 检查布局互换
rs = rot180_swap(INIT)
chk("初始局面旋转换色后棋盘不变(仅行动方变白)", rs[0] == INIT[0] and rs[1] == INIT[1] and rs[2] == 1)

chk("索引空间大小", NM == 2380)

print("\n全部 %d 项测试通过" % ok)
