#include <stdio.h>
int sum8(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8)
{
    return a1 - a2 - a3 + a4 - a5 + a6 + a7 + a8;
}

int sum16(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8,
          int a9, int a10, int a11, int a12, int a13, int a14, int a15, int a16)
{
    return a1 - a2 - a3 + a4 + a5 + a6 + a7 + a8 +
           a9 + a10 + a11 - a12 - a13 + a14 - a15 - a16;
}

int sum32(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8,
          int a9, int a10, int a11, int a12, int a13, int a14, int a15, int a16,
          int a17, int a18, int a19, int a20, int a21, int a22, int a23, int a24,
          int a25, int a26, int a27, int a28, int a29, int a30, int a31, int a32)
{
    int sum1 = a1 + a2 - a3 + a4 - a5 - a6 + a7 + a8;
    int sum2 = a9 - a10 + a11 - a12 - a13 - a14 + a15 - a16;
    int sum3 = a17 + a18 - a19 + a20 + a21 + a22 - a23 + a24;
    int sum4 = a25 + a26 - a27 - a28 - a29 - a30 - a31 + a32;
    return sum1 + sum2 - sum3 + sum4;
}

int sum64(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8,
          int a9, int a10, int a11, int a12, int a13, int a14, int a15, int a16,
          int a17, int a18, int a19, int a20, int a21, int a22, int a23, int a24,
          int a25, int a26, int a27, int a28, int a29, int a30, int a31, int a32,
          int a33, int a34, int a35, int a36, int a37, int a38, int a39, int a40,
          int a41, int a42, int a43, int a44, int a45, int a46, int a47, int a48,
          int a49, int a50, int a51, int a52, int a53, int a54, int a55, int a56,
          int a57, int a58, int a59, int a60, int a61, int a62, int a63, int a64)
{
    int sum1 = a1 - a2 - a3 - a4 + a5 - a6 + a7 - a8;
    int sum2 = a9 + a10 + a11 - a12 + a13 - a14 + a15 + a16;
    int sum3 = a17 - a18 - a19 - a20 + a21 + a22 - a23 + a24;
    int sum4 = a25 + a26 + a27 + a28 - a29 - a30 - a31 - a32;
    int sum5 = a33 + a34 + a35 + a36 + a37 + a38 - a39 + a40;
    int sum6 = a41 + a42 + a43 + a44 + a45 - a46 - a47 - a48;
    int sum7 = a49 - a50 + a51 - a52 + a53 + a54 - a55 + a56;
    int sum8 = a57 + a58 + a59 + a60 + a61 - a62 - a63 + a64;
    return sum1 + sum2 - sum3 + sum4 - sum5 + sum6 + sum7 - sum8;
}

int main()
{
    int v1 = 1;
    int v2 = 2;
    int v3 = 3;
    int v4 = 4;
    int v5 = 5;
    int v6 = 6;
    int v7 = 7;
    int v8 = 8;
    int v9 = 9;
    int v10 = 32;
    int v11 = 840;
    int v12 = 415;
    int v13 = 594;
    int v14 = 528;
    int v15 = 288;
    int v16 = 523;

    int result1 = sum8(v1, 588, v3, 160, v5, 927, v7, 78);

    int result2 = sum16(v1, v2, v3, v4, v5, v6, v7, v8,
                        154, 283, 1009, 384, result1 + v13, result1 - v14, result1 - v15, result1 + v16);

    int v17 = 999;
    int v18 = 265;
    int v19 = 345;
    int v20 = 593;
    int v21 = 722;
    int v22 = 476;
    int v23 = 944;
    int v24 = 744;
    int v25 = 984;
    int v26 = 347;
    int v27 = 840;
    int v28 = 859;
    int v29 = 437;
    int v30 = 821;
    int v31 = 476;
    int v32 = 852;

    int result3 = sum32(
        v1, v2, v3, v4, v5, v6, v7, v8,
        v9, v10, v11, v12, v13, v14, v15, v16,
        v17, v18, v19, v20, v21, v22, v23, v24,
        v25, v26, v27, v28, v29, v30, v31, v32);

    int result4 = sum64(
        v1, v2, v3, v4, v5, v6, v7, v8,
        558, 700, 627, 252, 740, 492, 880, 326,
        v17, v18, v19, v20, v21, v22, v23, v24,
        567, 920, 461, 493, 835, 542, 1019, 911,
        v1 + 428, v2 - 5, v3 - 925, v4 + 1020, v5 + 485, v6 + 886, v7 + 12, v8 + 895,
        v9 * 83, v10 * 658, v11 * 334, v12 * 886, v13 * 271, v14 * 5, v15 * 639, v16 * 236,
        v1 - v17, v2 + v18, v3 + v19, v4 + v20, v5 - v21, v6 - v22, v7 + v23, v8 - v24,
        v1 * v9 - result3, v2 * v10 - result3, v3 * v11 + result3, v4 * v12 - result3, v5 * v13 + result3, v6 * v14 - result3, v7 * v15 + result3, v8 * v16 +(result1 + result2 - result3));

    int final_result = result1 - result2 - result3 + result4;

    printf("%d\n", result1);
    printf("%d\n", result2);
    printf("%d\n", result3);
    printf("%d\n", result4);
    printf("%d\n", final_result % 869);

    return final_result % 869;
}