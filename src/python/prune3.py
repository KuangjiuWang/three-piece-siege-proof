# -*- coding: utf-8 -*-
"""形状白名单贪心最小化(论文证书版): 要求 (INIT,黑先) 与 (INIT,白先) 同时安全,
这样单个证书 S 同时给出黑方不败(S)与白方不败(rho(S))。输出 allowed2.bin / safe6.bin。"""
import subprocess, pickle, os, time
from engine import *

RMASK = 0xFF60
T0 = time.time()

def mirror_mask(m):
    out = 0
    for s in bits(m): out |= 1 << ((s // 4) * 4 + (3 - s % 4))
    return out

def run_safety(allowed_ranks):
    arr = bytearray(2380)
    for r in allowed_ranks: arr[r] = 1
    open("allowed_try3.bin", "wb").write(bytes(arr))
    out = subprocess.run(["./safety6", "allowed_try3.bin"], capture_output=True, text=True)
    return "BOTH SAFE = 1" in out.stdout

STATE = "prune3_state.pkl"
if os.path.exists(STATE) and not os.environ.get("FRESH"):
    allowed, pairs, idx = pickle.load(open(STATE, "rb"))
else:
    all_shapes = [m for m in MASKS34 if not (m & ~RMASK & 0xFFFF)]
    def spread(m):
        ps = list(bits(m))
        return sum(abs(a//4-b//4)+abs(a%4-b%4) for a in ps for b in ps)
    HOMEsq = [sq_of(3,2), sq_of(3,3), sq_of(4,2), sq_of(4,3)]
    def score(m):
        ps = list(bits(m))
        d = sum(min(abs(p//4-h//4)+abs(p%4-h%4) for h in HOMEsq) for p in ps)
        return (d + spread(m), m)
    pairs, done = [], set()
    for m in sorted(all_shapes, key=score, reverse=True):
        cm = min(m, mirror_mask(m))
        if cm in done: continue
        done.add(cm)
        pairs.append((cm, mirror_mask(cm)))
    allowed = set(RANK[m] for m in all_shapes)
    idx = 0
    assert run_safety(allowed), "全区域形状集必须满足 BOTH SAFE"

while idx < len(pairs):
    m1, m2 = pairs[idx]
    trial = allowed - {RANK[m1], RANK[m2]}
    if trial and run_safety(trial):
        allowed = trial
    idx += 1
    if idx % 20 == 0:
        pickle.dump((allowed, pairs, idx), open(STATE, "wb"))
        print("进度 %d/%d 余%d (%.0fs)" % (idx, len(pairs), len(allowed), time.time() - T0), flush=True)

pickle.dump((allowed, pairs, idx), open(STATE, "wb"))
assert run_safety(allowed)
arr = bytearray(2380)
for r in allowed: arr[r] = 1
open("allowed2.bin", "wb").write(bytes(arr))
print(subprocess.run(["./safety6", "allowed2.bin", "save"], capture_output=True, text=True).stdout)
shp = sorted(MASKS34[r] for r in allowed)
pickle.dump(shp, open("allowed2_shapes.pkl", "wb"))
c4 = sum(1 for m in shp if popcount(m) == 4)
print("DONE 剩余形状=%d (4子:%d, 3子:%d)" % (len(shp), c4, len(shp) - c4))
donep, cn = set(), []
for m in shp:
    cm = min(m, mirror_mask(m))
    if cm not in donep:
        donep.add(cm); cn.append(cm)
print("canonical=%d" % len(cn))
for m in sorted(cn, key=lambda x: (-popcount(x), x)):
    print(" ", coords_of(m))
