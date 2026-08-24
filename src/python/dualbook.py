# -*- coding: utf-8 -*-
"""双色规则书运行库: 同一本书 (book_dual.pkl) 既可给黑方用, 也可经色反演 rho 给白方用。

rho(B, W, t) = (tau W, tau B, 1 - t),  tau = 180 度旋转 (sq -> 15 - sq)。
白方策略 = rho o (黑方策略) o rho。
"""
import pickle, os
from engine import *

_DIR = os.path.dirname(os.path.abspath(__file__))
BOOK = pickle.load(open(os.path.join(_DIR, "book_dual.pkl"), "rb"))
ALLOW = open(os.path.join(_DIR, "allowed2.bin"), "rb").read()
RMASK = 0xFF60

# ---------- 两个盘面对称: mu = 左右镜像(保色), tau = 180度旋转(用于换色) ----------
def mu_sq(s):  return (s // 4) * 4 + (3 - s % 4)
def tau_sq(s): return 15 - s
def _mask(m, f):
    out = 0
    for s in bits(m): out |= 1 << f(s)
    return out
def mu_mask(m):  return _mask(m, mu_sq)
def tau_mask(m): return _mask(m, tau_sq)

def rho(state):
    B, W, t = state
    return (tau_mask(W), tau_mask(B), t ^ 1)

def canon(state):
    """左右镜像归一(书只存字典序较小的一侧)。"""
    B, W, t = state
    Bm, Wm = mu_mask(B), mu_mask(W)
    if (Bm, Wm) < (B, W): return (Bm, Wm, t), True
    return state, False

def allowed_mask(m):
    return not (m & ~RMASK & 0xFFFF) and ALLOW[RANK[m]] == 1

# ---------- 强制吃子的固定偏好(黑方视角) ----------
def cap_pref(zs):
    """双吃候选时: 吃行号更大者(更靠近黑方底线); 同行时吃列号更小者。"""
    return max(zs, key=lambda z: (z // 4, -(z % 4)))

def cap_pref_white(zs):
    """白方视角的偏好 = tau 共轭后的同一规则(保证 rho 把黑策略精确搬给白方)。"""
    return tau_sq(cap_pref([tau_sq(z) for z in zs]))

# ---------- 动作执行 ----------
def apply_move(state, mv, pref):
    """行动方走 mv=(x,y); 返回 ('win',) / ('s', ns) / None(非法)。"""
    B, W, t = state
    x, y = mv
    P, Q = (B, W) if t == 0 else (W, B)
    if not ((P >> x) & 1): return None
    if ((B | W) >> y) & 1: return None
    if not ((NEI_MASK[x] >> y) & 1): return None
    P2 = (P ^ (1 << x)) | (1 << y)
    zs = capture_candidates(P2, Q, y)
    if not zs:
        B2, W2 = (P2, Q) if t == 0 else (Q, P2)
        return ('s', (B2, W2, t ^ 1))
    Q2 = Q ^ (1 << pref(zs))
    if popcount(Q2) == 2: return ('win',)
    B2, W2 = (P2, Q2) if t == 0 else (Q2, P2)
    return ('s', (B2, W2, t ^ 1))

def apply_black(state, mv): return apply_move(state, mv, cap_pref)
def apply_white(state, mv): return apply_move(state, mv, cap_pref_white)

# ---------- 谓词 ----------
def atom_eval(atom, W, B=0):
    k = atom[0]
    if k == 'wsq':  return ((W >> atom[1]) & 1) == 1
    if k == 'nwsq': return ((W >> atom[1]) & 1) == 0
    if k == 'nw':   return popcount(W) == atom[1]
    if k == 'colge': return popcount(W & COL_MASK[atom[1]]) >= atom[2]
    if k == 'colle': return popcount(W & COL_MASK[atom[1]]) <= atom[2]
    if k == 'rowge': return popcount(W & ROW_MASK[atom[1] * 4]) >= atom[2]
    if k == 'rowle': return popcount(W & ROW_MASK[atom[1] * 4]) <= atom[2]
    if k == 'thr':   return any(a[2] is not None for a in successors((B, W, 1)))
    if k == 'nthr':  return not any(a[2] is not None for a in successors((B, W, 1)))
    raise ValueError(atom)

def atom_en(atom):
    k = atom[0]
    if k == 'wsq':  return "opp on %d%d" % rc(atom[1])
    if k == 'nwsq': return "opp not on %d%d" % rc(atom[1])
    if k == 'nw':   return "opp has %d men" % atom[1]
    if k == 'colge': return "col %d has >=%d opp" % (atom[1] + 1, atom[2])
    if k == 'colle': return "col %d empty of opp" % (atom[1] + 1)
    if k == 'rowge': return "row %d has >=%d opp" % (atom[1] + 1, atom[2])
    if k == 'rowle': return "row %d empty of opp" % (atom[1] + 1)
    if k == 'thr':  return "opp has a capturing move"
    if k == 'nthr': return "opp has no capturing move"
    return str(atom)

# ---------- 查书(黑方视角) ----------
def black_move_ex(state):
    """state 必须是黑方行动。返回 (走法, 解释)。"""
    cst, flip = canon(state)
    B, W, t = cst
    items = BOOK.get(B)
    if items is None: return None, "shape not in book"
    for i, it in enumerate(items):
        if it[0] == 'mv':
            m = it[1]
            r = apply_black(cst, m)
            if r is None: continue
            if r != ('win',) and not allowed_mask(r[1][0]): continue
            why = "shape %s rule %d (candidate)" % (coords_of(B), i + 1)
            return (mu_mv(m) if flip else m), why + (" [mirrored]" if flip else "")
        else:
            _, conds, m = it
            if all(atom_eval(a, W, B) for a in conds):
                why = "shape %s rule %d: if [%s]" % (coords_of(B), i + 1,
                                                     " and ".join(atom_en(a) for a in conds))
                return (mu_mv(m) if flip else m), why + (" [mirrored]" if flip else "")
    return None, "not covered (should not happen)"

def mu_mv(mv): return (mu_sq(mv[0]), mu_sq(mv[1]))
def tau_mv(mv): return (tau_sq(mv[0]), tau_sq(mv[1]))

def white_move_ex(state):
    """state 必须是白方行动。白方策略 = rho 共轭的同一本书。"""
    mv, why = black_move_ex(rho(state))
    if mv is None: return None, why
    return tau_mv(mv), why + " [colour-reversed]"

def black_move(state): return black_move_ex(state)[0]
def white_move(state): return white_move_ex(state)[0]
