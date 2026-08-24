/* 4x4 三子夹击棋 v2 —— 全状态空间求解器(独立于 Python 的第二套规则实现)
 * 状态: (B, W, t), |B|,|W| ∈ {3,4}, t=0 黑行动 / 1 白行动
 * 语义: 环 = 平局(配合三次重复规则, 任何无限对局必然三次重复判和)
 * 值(对当前行动方): 1=WIN, 2=LOSE, 3=DRAW, 0=未定(迭代中), 255=非法
 *
 * 用法:  ./solve solve            求解并写出 values.bin
 *        ./solve succ < in.txt    每行 "B W t"(十进制), 输出动作/后继列表用于交叉验证
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
static uint8_t *val;

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

/* 检查移动后经过 y 的两条线, 返回吃子候选 z 的格号, 最多 2 个 */
static inline int captures(uint16_t P2, uint16_t Q, int y, int *zs) {
    int n = 0;
    uint16_t lines[2] = { ROWM[y], COLM[y] };
    for (int li = 0; li < 2; li++) {
        uint16_t L = lines[li];
        uint16_t o = (uint16_t)((P2 | Q) & L);
        if (pc16(o) != 3) continue;
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

static inline int side_has_move(uint16_t P, uint16_t occ) {
    uint16_t m = P;
    while (m) {
        int x = __builtin_ctz(m);
        m &= (uint16_t)(m - 1);
        if (NEI_MASK[x] & (uint16_t)~occ) return 1;
    }
    return 0;
}

/* 求一个状态的值(基于当前 val 表): 返回 1 WIN / 2 LOSE / 0 未定 */
static inline int eval_state(uint16_t B, uint16_t W, int t) {
    uint16_t P = t == 0 ? B : W, Q = t == 0 ? W : B;
    uint16_t occ = (uint16_t)(B | W);
    int all_win = 1, any = 0;
    uint16_t m = P;
    while (m) {
        int x = __builtin_ctz(m);
        m &= (uint16_t)(m - 1);
        uint16_t px = (uint16_t)(P ^ (1u << x));
        uint16_t free_ = (uint16_t)(NEI_MASK[x] & ~occ);
        uint16_t fm = free_;
        while (fm) {
            int y = __builtin_ctz(fm);
            fm &= (uint16_t)(fm - 1);
            any = 1;
            uint16_t P2 = (uint16_t)(px | (1u << y));
            int zs[2];
            int nz = captures(P2, Q, y, zs);
            if (nz == 0) {
                uint16_t B2 = t == 0 ? P2 : Q, W2 = t == 0 ? Q : P2;
                uint8_t v2 = val[IDX(RANK[B2], RANK[W2], t ^ 1)];
                if (v2 == 2) return 1;
                if (v2 != 1) all_win = 0;
            } else {
                for (int i = 0; i < nz; i++) {
                    uint16_t Q2 = (uint16_t)(Q ^ (1u << zs[i]));
                    if (pc16(Q2) == 2) return 1;   /* 吃至 2 子, 立即胜 */
                    uint16_t B2 = t == 0 ? P2 : Q2, W2 = t == 0 ? Q2 : P2;
                    uint8_t v2 = val[IDX(RANK[B2], RANK[W2], t ^ 1)];
                    if (v2 == 2) return 1;
                    if (v2 != 1) all_win = 0;
                }
            }
        }
    }
    if (!any) return 2;        /* 无合法移动 -> 当前方负 */
    if (all_win) return 2;     /* 所有动作都进入对方必胜 -> 当前方负 */
    return 0;
}

static uint16_t *lay;   /* 状态被判定时的同步 BFS 层号(0 = 初始判定) */
static uint8_t *valold;

static void do_solve(void) {
    size_t total = (size_t)NMASK * NMASK * 2;
    val = (uint8_t *)malloc(total);
    valold = (uint8_t *)malloc(total);
    lay = (uint16_t *)malloc(total * sizeof(uint16_t));
    memset(val, 0, total);
    memset(lay, 0xFF, total * sizeof(uint16_t));

    /* 初始化: 非法状态 / 无移动状态 */
    size_t n_valid = 0;
    for (int rb = 0; rb < NMASK; rb++) {
        uint16_t B = MASKS[rb];
        for (int rw = 0; rw < NMASK; rw++) {
            uint16_t W = MASKS[rw];
            if (B & W) { val[IDX(rb, rw, 0)] = 255; val[IDX(rb, rw, 1)] = 255; continue; }
            uint16_t occ = (uint16_t)(B | W);
            n_valid += 2;
            if (!side_has_move(B, occ)) { val[IDX(rb, rw, 0)] = 2; lay[IDX(rb, rw, 0)] = 0; }
            if (!side_has_move(W, occ)) { val[IDX(rb, rw, 1)] = 2; lay[IDX(rb, rw, 1)] = 0; }
        }
    }
    fprintf(stderr, "valid states: %zu\n", n_valid);

    /* 同步值迭代直至不动点: 每轮只读上一轮的表(valold), 层号=判定轮次。
       性质: WIN@k 存在一个动作到 立即胜 或 LOSE@<k; LOSE@k 的所有动作都到 WIN@<k。
       因此层号沿最优攻杀路线严格递减, 可用于兑现胜利。 */
    int sweep = 0;
    for (;;) {
        sweep++;
        size_t changed = 0;
        /* 快照: 两个缓冲区内容相同; eval_state 读全局 val(旧), 新值写入 valold */
        memcpy(valold, val, total);
        for (int rb = 0; rb < NMASK; rb++) {
            uint16_t B = MASKS[rb];
            for (int rw = 0; rw < NMASK; rw++) {
                uint16_t W = MASKS[rw];
                if (B & W) continue;
                for (int t = 0; t < 2; t++) {
                    size_t id = IDX(rb, rw, t);
                    if (val[id] != 0) continue;   /* 已判定, 两个缓冲区已一致 */
                    int v = eval_state(B, W, t);
                    if (v) { valold[id] = (uint8_t)v; lay[id] = (uint16_t)sweep; changed++; }
                }
            }
        }
        { uint8_t *tmp = val; val = valold; valold = tmp; }  /* val 换成新表 */
        fprintf(stderr, "sweep %d: changed %zu\n", sweep, changed);
        if (!changed) break;
    }

    size_t cw = 0, cl = 0, cd = 0;
    for (int rb = 0; rb < NMASK; rb++) {
        uint16_t B = MASKS[rb];
        for (int rw = 0; rw < NMASK; rw++) {
            uint16_t W = MASKS[rw];
            if (B & W) continue;
            for (int t = 0; t < 2; t++) {
                size_t id = IDX(rb, rw, t);
                if (val[id] == 0) { val[id] = 3; cd++; }
                else if (val[id] == 1) cw++;
                else if (val[id] == 2) cl++;
            }
        }
    }
    printf("sweeps=%d WIN=%zu LOSE=%zu DRAW=%zu\n", sweep, cw, cl, cd);

    uint16_t B0 = 0xF000, W0 = 0x000F;
    printf("initial value (side to move = Black): %d  (1=WIN 2=LOSE 3=DRAW)\n",
           val[IDX(RANK[B0], RANK[W0], 0)]);

    FILE *f = fopen("values.bin", "wb");
    fwrite(val, 1, total, f);
    fclose(f);
    f = fopen("layers.bin", "wb");
    fwrite(lay, sizeof(uint16_t), total, f);
    fclose(f);
    fprintf(stderr, "values.bin + layers.bin written\n");
}

/* 交叉验证模式: 对每行输入 "B W t" 输出规范化动作列表 */
static void do_succ(void) {
    unsigned b, w; int t;
    while (scanf("%u %u %d", &b, &w, &t) == 3) {
        uint16_t B = (uint16_t)b, W = (uint16_t)w;
        uint16_t P = t == 0 ? B : W, Q = t == 0 ? W : B;
        uint16_t occ = (uint16_t)(B | W);
        /* 收集动作: x,y,z(-1 无),win(0/1),B2,W2 */
        long long recs[128]; int nrec = 0;
        uint16_t m = P;
        while (m) {
            int x = __builtin_ctz(m); m &= (uint16_t)(m - 1);
            uint16_t px = (uint16_t)(P ^ (1u << x));
            uint16_t fm = (uint16_t)(NEI_MASK[x] & ~occ);
            while (fm) {
                int y = __builtin_ctz(fm); fm &= (uint16_t)(fm - 1);
                uint16_t P2 = (uint16_t)(px | (1u << y));
                int zs[2]; int nz = captures(P2, Q, y, zs);
                if (nz == 0) {
                    uint16_t B2 = t == 0 ? P2 : Q, W2 = t == 0 ? Q : P2;
                    /* 编码: ((x*100+y)*1000 + zcode)*2^32 + B2*65536 + W2, 无吃子 zcode=999 */
                    recs[nrec++] = (((long long)x * 100 + y) * 1000 + 999) * 4294967296LL +
                                   (long long)B2 * 65536 + W2;
                } else {
                    for (int i = 0; i < nz; i++) {
                        uint16_t Q2 = (uint16_t)(Q ^ (1u << zs[i]));
                        int win = pc16(Q2) == 2;
                        uint16_t B2 = t == 0 ? P2 : Q2, W2 = t == 0 ? Q2 : P2;
                        recs[nrec++] = (((long long)x * 100 + y) * 1000 + zs[i] * 10 + (win ? 1 : 0)) * 4294967296LL +
                                       (long long)B2 * 65536 + W2;
                    }
                }
            }
        }
        /* 排序输出 */
        for (int i = 0; i < nrec; i++)
            for (int j = i + 1; j < nrec; j++)
                if (recs[j] < recs[i]) { long long tmp = recs[i]; recs[i] = recs[j]; recs[j] = tmp; }
        printf("%d", nrec);
        for (int i = 0; i < nrec; i++) printf(" %lld", recs[i]);
        printf("\n");
    }
}

int main(int argc, char **argv) {
    init_tables();
    if (NM != NMASK) { fprintf(stderr, "NM=%d != %d\n", NM, NMASK); return 1; }
    if (argc > 1 && strcmp(argv[1], "succ") == 0) do_succ();
    else do_solve();
    return 0;
}
