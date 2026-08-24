# -*- coding: utf-8 -*-
"""
4x4 三子夹击棋 规则 v2 —— Python 参考实现(位棋盘)。
坐标: (r,c), r=1..4 从上到下, c=1..4 从左到右。格号 sq = (r-1)*4 + (c-1)。
状态: (B, W, t)  B/W 为 16 位掩码, t=0 黑方行动, t=1 白方行动。
"""

def sq_of(r, c):
    return (r - 1) * 4 + (c - 1)

def rc(sq):
    return (sq // 4 + 1, sq % 4 + 1)

NEI = []
for _s in range(16):
    _r, _c = _s // 4, _s % 4
    _n = []
    if _r > 0: _n.append(_s - 4)
    if _r < 3: _n.append(_s + 4)
    if _c > 0: _n.append(_s - 1)
    if _c < 3: _n.append(_s + 1)
    NEI.append(tuple(_n))
NEI_MASK = [sum(1 << x for x in n) for n in NEI]
ROW_MASK = [0xF << (4 * (s // 4)) for s in range(16)]
COL_MASK = [0x1111 << (s % 4) for s in range(16)]

INIT = (0xF000, 0x000F, 0)  # 黑在第4行, 白在第1行, 黑先

def popcount(x):
    return bin(x).count('1')

def bits(x):
    while x:
        b = x & -x
        yield b.bit_length() - 1
        x ^= b

def mask_of(coords):
    return sum(1 << sq_of(r, c) for (r, c) in coords)

def coords_of(mask):
    return [rc(s) for s in bits(mask)]

def capture_candidates(P2, Q, y):
    """移动完成后, 只检查经过目的地 y 的行与列。返回被吃格 z 的列表(0..2 个)。
    条件(规则 7.2): 线上恰 3 子; 行动方恰 2 子且相邻; 对方恰 1 子且与行动方某子相邻。"""
    out = []
    for L in (ROW_MASK[y], COL_MASK[y]):
        o = (P2 | Q) & L
        if popcount(o) != 3:
            continue
        p = P2 & L
        if popcount(p) != 2:
            continue
        q = Q & L
        if popcount(q) != 1:
            continue
        lo = p & -p
        a = lo.bit_length() - 1
        b = (p ^ lo).bit_length() - 1
        if not (NEI_MASK[a] >> b) & 1:
            continue
        z = (q & -q).bit_length() - 1
        if NEI_MASK[z] & p:
            out.append(z)
    return out

def successors(state):
    """列出当前行动方的全部动作 (x, y, z, result)。
    z: 被吃格号或 None。吃子是强制的: 有候选的移动只生成吃子动作(候选二选一 = 两个动作)。
    result: ('win',)            —— 对方被吃至 2 子, 行动方立即获胜(终局);
            ('s', (B2,W2,t2))   —— 正常进入下一完整局面。
    注意: 若下一局面的行动方无合法移动, 按规则由本回合行动方获胜, 由调用方判定。"""
    B, W, t = state
    P, Q = (B, W) if t == 0 else (W, B)
    occ = B | W
    res = []
    for x in bits(P):
        px = P ^ (1 << x)
        for y in NEI[x]:
            if (occ >> y) & 1:
                continue
            P2 = px | (1 << y)
            zs = capture_candidates(P2, Q, y)
            if not zs:
                B2, W2 = (P2, Q) if t == 0 else (Q, P2)
                res.append((x, y, None, ('s', (B2, W2, t ^ 1))))
            else:
                for z in zs:
                    Q2 = Q ^ (1 << z)
                    if popcount(Q2) == 2:
                        res.append((x, y, z, ('win',)))
                    else:
                        B2, W2 = (P2, Q2) if t == 0 else (Q2, P2)
                        res.append((x, y, z, ('s', (B2, W2, t ^ 1))))
    return res

def has_move(state):
    B, W, t = state
    P = B if t == 0 else W
    occ = B | W
    for x in bits(P):
        for y in NEI[x]:
            if not (occ >> y) & 1:
                return True
    return False

def board_str(B, W):
    rows = []
    for r in range(4):
        cells = []
        for c in range(4):
            s = r * 4 + c
            cells.append('B' if (B >> s) & 1 else ('W' if (W >> s) & 1 else '.'))
        rows.append(' '.join(cells))
    return '\n'.join(rows)

# ---- 对称变换(用于校验与策略化简) ----
def tf_mask(mask, f):
    m = 0
    for s in bits(mask):
        r, c = rc(s)
        m |= 1 << sq_of(*f(r, c))
    return m

def mirror_lr(state):   # 左右镜像 c -> 5-c (不换色, 不换行动方)
    B, W, t = state
    f = lambda r, c: (r, 5 - c)
    return (tf_mask(B, f), tf_mask(W, f), t)

def rot180_swap(state):  # 180 度旋转 + 黑白互换 + 行动方互换 (规则完全对称)
    B, W, t = state
    f = lambda r, c: (5 - r, 5 - c)
    return (tf_mask(W, f), tf_mask(B, f), t ^ 1)

# ---- 状态索引(与 C 求解器完全一致) ----
MASKS34 = [m for m in range(65536) if popcount(m) in (3, 4)]
RANK = {m: i for i, m in enumerate(MASKS34)}
NM = len(MASKS34)  # 2380

def sidx(state):
    B, W, t = state
    return (RANK[B] * NM + RANK[W]) * 2 + t

# 数值含义(对"当前行动方"而言): 1=必胜 WIN, 2=必败 LOSE, 3=平局 DRAW, 255=非法
V_WIN, V_LOSE, V_DRAW, V_INVALID = 1, 2, 3, 255
