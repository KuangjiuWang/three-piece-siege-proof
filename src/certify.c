/* certify.c -- 论文主定理的独立证书检验器。
 *
 * 输入: 一个证书位集文件 (默认 safe6.bin), 每字节 0/1, 索引 IDX(rank(B),rank(W),t)。
 * 本程序 *不做任何不动点迭代*, 不读取胜负表; 只做纯局部检验:
 *
 *  (C1) 合法性: S 中每个状态满足 B&W==0, |B|,|W| ∈ {3,4}。
 *  (C2) 黑方安全闭合 (Black-safety):
 *        t=0: 存在黑动作 —— 立即吃白至2子(黑胜), 或后继 ∈ S;
 *        t=1: 所有白动作 —— 都不是"吃黑至2子", 且后继 ∈ S。(白无着 => 黑胜, 合格)
 *  (C3) INIT = (0xF000,0x000F,0) ∈ S。
 *  (C4) 令 rho(B,W,t) = (rot180(W), rot180(B), 1-t) (规则的换色自同构),
 *        T = {INIT} ∪ rho(S)。检验 T 的白方安全闭合 (White-safety):
 *        t=1: 存在白动作 —— 立即吃黑至2子(白胜), 或后继 ∈ T;
 *        t=0: 所有黑动作 —— 都不是"吃白至2子", 且后继 ∈ T。(黑无着 => 白胜, 合格)
 *
 * (C1)-(C3) => 黑方从 INIT 永不落败;  (C1)(C4) => 白方从 INIT 永不落败。
 * 二者合并 => 初始局面双方均无必胜策略 => 平局。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define NMASK 2380
#define IDX(rb, rw, t) (((((size_t)(rb)) * NMASK + (size_t)(rw)) << 1) | (size_t)(t))

static int NEI_CNT[16], NEI_LIST[16][4];
static uint16_t NEI_MASK[16], ROWM[16], COLM[16];
static int32_t RANK[65536];
static uint16_t MASKS[NMASK], ROT[65536];
static int NM = 0;
static uint8_t *S;      /* 证书 */

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
    for (int m = 0; m < 65536; m++) {
        uint16_t r = 0;
        for (int s = 0; s < 16; s++) if ((m >> s) & 1) r |= (uint16_t)(1u << (15 - s));
        ROT[m] = r;
    }
}

/* 规则 7.2: 移动方 P2 刚走到 y, 对方 Q。返回可吃格数(0..2)。 */
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

/* ---------- 证书成员测试 ---------- */
static inline int inS(uint16_t B, uint16_t W, int t) {
    if (B & W) return 0;
    if (RANK[B] < 0 || RANK[W] < 0) return 0;
    return S[IDX(RANK[B], RANK[W], t)];
}
/* T = {INIT} ∪ rho(S);  rho 是对合: rho(B,W,t) = (rot W, rot B, 1-t) */
static inline int inT(uint16_t B, uint16_t W, int t) {
    if (B == 0xF000u && W == 0x000Fu && t == 0) return 1;
    return inS(ROT[W], ROT[B], t ^ 1);
}

/* ---------- (C2) 黑方安全闭合 ---------- */
static int check_black_to_move(uint16_t B, uint16_t W) {   /* 需存在安全动作 */
    uint16_t occ = (uint16_t)(B | W), m = B;
    while (m) {
        int x = __builtin_ctz(m); m &= (uint16_t)(m - 1);
        uint16_t px = (uint16_t)(B ^ (1u << x));
        uint16_t fm = (uint16_t)(NEI_MASK[x] & ~occ);
        while (fm) {
            int y = __builtin_ctz(fm); fm &= (uint16_t)(fm - 1);
            uint16_t B2 = (uint16_t)(px | (1u << y));
            int zs[2], nz = captures(B2, W, y, zs);
            if (nz == 0) { if (inS(B2, W, 1)) return 1; }
            else for (int i = 0; i < nz; i++) {
                uint16_t W2 = (uint16_t)(W ^ (1u << zs[i]));
                if (pc16(W2) == 2) return 1;             /* 黑立即取胜 */
                if (inS(B2, W2, 1)) return 1;
            }
        }
    }
    return 0;
}
static int check_white_to_move(uint16_t B, uint16_t W) {   /* 需所有动作安全 */
    uint16_t occ = (uint16_t)(B | W), m = W;
    while (m) {
        int x = __builtin_ctz(m); m &= (uint16_t)(m - 1);
        uint16_t px = (uint16_t)(W ^ (1u << x));
        uint16_t fm = (uint16_t)(NEI_MASK[x] & ~occ);
        while (fm) {
            int y = __builtin_ctz(fm); fm &= (uint16_t)(fm - 1);
            uint16_t W2 = (uint16_t)(px | (1u << y));
            int zs[2], nz = captures(W2, B, y, zs);
            if (nz == 0) { if (!inS(B, W2, 0)) return 0; }
            else for (int i = 0; i < nz; i++) {
                uint16_t B2 = (uint16_t)(B ^ (1u << zs[i]));
                if (pc16(B2) == 2) return 0;             /* 黑被吃至2子 => 违反 */
                if (!inS(B2, W2, 0)) return 0;
            }
        }
    }
    return 1;   /* 白无着 => 黑胜 */
}

/* ---------- (C4) 白方安全闭合 (在 T 上) ---------- */
static int checkT_white_to_move(uint16_t B, uint16_t W) {  /* 需存在安全动作 */
    uint16_t occ = (uint16_t)(B | W), m = W;
    while (m) {
        int x = __builtin_ctz(m); m &= (uint16_t)(m - 1);
        uint16_t px = (uint16_t)(W ^ (1u << x));
        uint16_t fm = (uint16_t)(NEI_MASK[x] & ~occ);
        while (fm) {
            int y = __builtin_ctz(fm); fm &= (uint16_t)(fm - 1);
            uint16_t W2 = (uint16_t)(px | (1u << y));
            int zs[2], nz = captures(W2, B, y, zs);
            if (nz == 0) { if (inT(B, W2, 0)) return 1; }
            else for (int i = 0; i < nz; i++) {
                uint16_t B2 = (uint16_t)(B ^ (1u << zs[i]));
                if (pc16(B2) == 2) return 1;             /* 白立即取胜 */
                if (inT(B2, W2, 0)) return 1;
            }
        }
    }
    return 0;
}
static int checkT_black_to_move(uint16_t B, uint16_t W) {  /* 需所有动作安全 */
    uint16_t occ = (uint16_t)(B | W), m = B;
    while (m) {
        int x = __builtin_ctz(m); m &= (uint16_t)(m - 1);
        uint16_t px = (uint16_t)(B ^ (1u << x));
        uint16_t fm = (uint16_t)(NEI_MASK[x] & ~occ);
        while (fm) {
            int y = __builtin_ctz(fm); fm &= (uint16_t)(fm - 1);
            uint16_t B2 = (uint16_t)(px | (1u << y));
            int zs[2], nz = captures(B2, W, y, zs);
            if (nz == 0) { if (!inT(B2, W, 1)) return 0; }
            else for (int i = 0; i < nz; i++) {
                uint16_t W2 = (uint16_t)(W ^ (1u << zs[i]));
                if (pc16(W2) == 2) return 0;             /* 白被吃至2子 => 违反 */
                if (!inT(B2, W2, 1)) return 0;
            }
        }
    }
    return 1;   /* 黑无着 => 白胜 */
}

int main(int argc, char **argv) {
    const char *fn = (argc > 1) ? argv[1] : "safe6.bin";
    init_tables();
    size_t total = (size_t)NMASK * NMASK * 2;
    S = (uint8_t *)malloc(total);
    FILE *f = fopen(fn, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", fn); return 1; }
    if (fread(S, 1, total, f) != total) { fprintf(stderr, "bad size\n"); return 1; }
    fclose(f);

    size_t nS = 0, nT = 0, bad1 = 0, bad2 = 0, bad4 = 0;
    size_t nS_b = 0, nS_w = 0, nS4 = 0, nS3 = 0;
    uint8_t shape_used[NMASK]; memset(shape_used, 0, sizeof shape_used);

    for (int rb = 0; rb < NM; rb++) {
        uint16_t B = MASKS[rb];
        for (int rw = 0; rw < NM; rw++) {
            uint16_t W = MASKS[rw];
            for (int t = 0; t < 2; t++) {
                if (S[IDX(rb, rw, t)]) {
                    /* (C1) */
                    if (B & W) { bad1++; continue; }
                    nS++; shape_used[rb] = 1;
                    if (t == 0) nS_b++; else nS_w++;
                    if (pc16(B) == 4) nS4++; else nS3++;
                    /* (C2) */
                    if (t == 0) { if (!check_black_to_move(B, W)) bad2++; }
                    else        { if (!check_white_to_move(B, W)) bad2++; }
                }
                /* (C4): 在 T 上检验 */
                if (inT(B, W, t)) {
                    nT++;
                    if (t == 1) { if (!checkT_white_to_move(B, W)) bad4++; }
                    else        { if (!checkT_black_to_move(B, W)) bad4++; }
                }
            }
        }
    }
    int nshape = 0;
    for (int rb = 0; rb < NM; rb++) nshape += shape_used[rb];

    printf("certificate file          : %s\n", fn);
    printf("|S|                       : %zu   (黑行动 %zu / 白行动 %zu; |B|=4: %zu, |B|=3: %zu)\n",
           nS, nS_b, nS_w, nS4, nS3);
    printf("S 中出现的黑方形状数      : %d\n", nshape);
    printf("|T| = |{INIT} u rho(S)|   : %zu\n", nT);
    printf("(C1) 非法状态             : %zu\n", bad1);
    printf("(C2) 黑方安全闭合违例     : %zu\n", bad2);
    int init_black = inS(0xF000u, 0x000Fu, 0);
    int init_white = inS(0xF000u, 0x000Fu, 1);
    printf("(C3) both initial states in S : %d\n", init_black && init_white);
    printf("(C4) 白方安全闭合违例     : %zu\n", bad4);
    int ok = (bad1 == 0 && bad2 == 0 && bad4 == 0 && init_black && init_white);
    printf("\n%s\n", ok ? "ALL CERTIFICATE CHECKS PASSED  ==> 初始局面为平局(双方均有不败策略)"
                        : "CERTIFICATE INVALID");
    return ok ? 0 : 1;
}
