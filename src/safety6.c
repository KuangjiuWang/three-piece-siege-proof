/* 两层嵌套安全博弈(全程不依赖胜负表):
 * 层4 (|B|=4): 黑行动 keep <=> 存在动作: 立即胜 或 后继 ∈ SAFE
 *              白行动 keep <=> 对白的每个动作:
 *                  - 非吃子: 后继 ∈ SAFE4
 *                  - 吃子(黑->3): 后继(轮黑,3子) ∈ SAFE3   ("毒饵/可反杀")
 *              (白无棋可走 => keep, 黑胜)
 * 层3 (|B|=3): 黑行动 keep <=> 存在动作: 立即胜 或 后继 ∈ SAFE3
 *              白行动 keep <=> 白没有任何吃子动作 且 所有后继 ∈ SAFE3
 * 从全体合法状态开始迭代删除, 求最大不动点; 检查初始局面是否在 SAFE4。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int NEI_CNT[16];
static int NEI_LIST[16][4];
static uint16_t NEI_MASK[16], ROWM[16], COLM[16];
static int32_t RANK[65536];
static uint16_t MASKS[2380];
static int NM = 0;
static uint8_t *safe;
#define RMASK ((uint16_t)0xFF60)
static uint8_t ALLOW[2380];

#define NMASK 2380
#define IDX(rb, rw, t) (((((size_t)(rb)) * NMASK + (size_t)(rw)) << 1) | (size_t)(t))
static inline int pc16(uint32_t x) { return __builtin_popcount(x); }

static void init_tables(void) {
    for (int s = 0; s < 16; s++) {
        int r = s / 4, c = s % 4, k = 0;
        if (r > 0) NEI_LIST[s][k++] = s - 4;
        if (r < 3) NEI_LIST[s][k++] = s + 4;
        if (c > 0) NEI_LIST[s][k++] = s - 1;
        if (c < 3) NEI_LIST[s][k++] = s + 1;
        NEI_CNT[s] = k;
        NEI_MASK[s] = 0;
        for (int i = 0; i < k; i++) NEI_MASK[s] |= (uint16_t)(1u << NEI_LIST[s][i]);
        ROWM[s] = (uint16_t)(0xFu << (4 * r));
        COLM[s] = (uint16_t)(0x1111u << c);
    }
    for (int m = 0; m < 65536; m++) RANK[m] = -1;
    for (int m = 0; m < 65536; m++) {
        int p = pc16((uint32_t)m);
        if (p == 3 || p == 4) { RANK[m] = NM; MASKS[NM++] = (uint16_t)m; }
    }
}

static inline int captures(uint16_t P2, uint16_t Q, int y, int *zs) {
    int n = 0;
    uint16_t lines[2] = { ROWM[y], COLM[y] };
    for (int li = 0; li < 2; li++) {
        uint16_t L = lines[li];
        if (pc16((uint16_t)((P2 | Q) & L)) != 3) continue;
        uint16_t p = (uint16_t)(P2 & L);
        if (pc16(p) != 2) continue;
        uint16_t q = (uint16_t)(Q & L);
        if (pc16(q) != 1) continue;
        int a = __builtin_ctz(p);
        int b = 31 - __builtin_clz((uint32_t)p);
        if (!((NEI_MASK[a] >> b) & 1)) continue;
        int z = __builtin_ctz(q);
        if (!(NEI_MASK[z] & p)) continue;
        zs[n++] = z;
    }
    return n;
}

/* 黑行动 keep: 存在安全动作 (两层通用) */
static int black_keep(uint16_t B, uint16_t W) {
    uint16_t occ = (uint16_t)(B | W);
    uint16_t m = B;
    while (m) {
        int x = __builtin_ctz(m); m &= (uint16_t)(m - 1);
        uint16_t px = (uint16_t)(B ^ (1u << x));
        uint16_t fm = (uint16_t)(NEI_MASK[x] & ~occ);
        while (fm) {
            int y = __builtin_ctz(fm); fm &= (uint16_t)(fm - 1);
            uint16_t B2 = (uint16_t)(px | (1u << y));
            if (B2 & (uint16_t)~RMASK) continue;
            if (!ALLOW[RANK[B2]]) continue;        /* 黑形状白名单 */
            int zs[2]; int nz = captures(B2, W, y, zs);
            if (nz == 0) {
                if (safe[IDX(RANK[B2], RANK[W], 1)]) return 1;
            } else {
                int z = zs[0];
                if (nz == 2 && (zs[1] / 4) > (z / 4)) z = zs[1];  /* 固定偏好: 吃行号更大者 */
                uint16_t W2 = (uint16_t)(W ^ (1u << z));
                if (pc16(W2) == 2) return 1;
                if (safe[IDX(RANK[B2], RANK[W2], 1)]) return 1;
            }
        }
    }
    return 0;
}

/* 白行动 keep */
static int white_keep(uint16_t B, uint16_t W) {
    int nb = pc16(B);
    uint16_t occ = (uint16_t)(B | W);
    uint16_t m = W;
    while (m) {
        int x = __builtin_ctz(m); m &= (uint16_t)(m - 1);
        uint16_t px = (uint16_t)(W ^ (1u << x));
        uint16_t fm = (uint16_t)(NEI_MASK[x] & ~occ);
        while (fm) {
            int y = __builtin_ctz(fm); fm &= (uint16_t)(fm - 1);
            uint16_t W2 = (uint16_t)(px | (1u << y));
            int zs[2]; int nz = captures(W2, B, y, zs);
            if (nz == 0) {
                if (!safe[IDX(RANK[B], RANK[W2], 0)]) return 0;
            } else {
                if (nb == 3) return 0;   /* 层3: 白绝不允许再有吃子机会 */
                for (int i = 0; i < nz; i++) {
                    uint16_t B2 = (uint16_t)(B ^ (1u << zs[i]));
                    /* 白吃黑到3子: 后继(轮黑)必须在 SAFE3 */
                    if (!safe[IDX(RANK[B2], RANK[W2], 0)]) return 0;
                }
            }
        }
    }
    return 1;  /* 白困毙 => 黑胜 */
}

int main(int argc, char **argv) {
    init_tables();
    { FILE *af = fopen(argc > 1 ? argv[1] : "allowed.bin", "rb");
      if (!af || fread(ALLOW, 1, 2380, af) != 2380) { fprintf(stderr, "bad allowed.bin\n"); return 2; }
      fclose(af); }
    size_t total = (size_t)NMASK * NMASK * 2;
    safe = (uint8_t *)malloc(total);
    memset(safe, 0, total);
    for (int rb = 0; rb < NMASK; rb++) {
        uint16_t B = MASKS[rb];
        if (!ALLOW[rb]) continue;
        for (int rw = 0; rw < NMASK; rw++) {
            uint16_t W = MASKS[rw];
            if (B & W) continue;
            if (B & (uint16_t)~RMASK) continue;
            if (!ALLOW[rb]) continue;
            safe[IDX(rb, rw, 0)] = 1;
            safe[IDX(rb, rw, 1)] = 1;
        }
    }
    int sweep = 0;
    for (;;) {
        sweep++;
        size_t removed = 0;
        for (int rb = 0; rb < NMASK; rb++) {
            uint16_t B = MASKS[rb];
        if (!ALLOW[rb]) continue;
            for (int rw = 0; rw < NMASK; rw++) {
                uint16_t W = MASKS[rw];
                if (B & W) continue;
                size_t i1 = IDX(rb, rw, 1), i0 = IDX(rb, rw, 0);
                if (safe[i1] && !white_keep(B, W)) { safe[i1] = 0; removed++; }
                if (safe[i0] && !black_keep(B, W)) { safe[i0] = 0; removed++; }
            }
        }

        if (!removed) break;
    }
    size_t c40 = 0, c41 = 0, c30 = 0, c31 = 0;
    for (int rb = 0; rb < NMASK; rb++) {
        uint16_t B = MASKS[rb];
        if (!ALLOW[rb]) continue;
        for (int rw = 0; rw < NMASK; rw++) {
            uint16_t W = MASKS[rw];
            if (B & W) continue;
            if (pc16(B) == 4) {
                c40 += safe[IDX(rb, rw, 0)];
                c41 += safe[IDX(rb, rw, 1)];
            } else {
                c30 += safe[IDX(rb, rw, 0)];
                c31 += safe[IDX(rb, rw, 1)];
            }
        }
    }
    printf("sweeps=%d SAFE4黑=%zu SAFE4白=%zu SAFE3黑=%zu SAFE3白=%zu\n",
           sweep, c40, c41, c30, c31);
    uint16_t B0 = 0xF000, W0 = 0x000F;
    printf("BOTH SAFE = %d\n", safe[IDX(RANK[B0], RANK[W0], 0)] && safe[IDX(RANK[B0], RANK[W0], 1)]);
    if (argc > 2) {           /* 只有带 save 参数才写盘 */
        FILE *f = fopen("safe6.bin", "wb");
        fwrite(safe, 1, total, f);
        fclose(f);
    }
    return 0;
}
