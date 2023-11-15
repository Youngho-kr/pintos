/* Pintos kernel does not support floating-point arithmetic 
   Fixed-point foramt is used instead of floating-point arithmetic
   
   Format:
   p integer
   q fraction
   For 32-bit, 1 bit for sign, 
   17 bits for integer, 14 bits for fraction
*/
#ifndef FIXED_POINT_H
#define FIXED_POINT_H

#define FIXED_P 17
#define FIXED_Q 14
#define FIXED_F 1<<(FIXED_Q)

typedef int FIXED;
/* Initial fixed-point
   Same as divided by zero
*/
static FIXED F_INIT(int a) {
    return (int32_t)(a * (FIXED_F));
}
static FIXED F_ADD(FIXED a, FIXED b) {
    return a + b;
}
static FIXED F_SUB(FIXED a, FIXED b) {
    return a - b;
}
static FIXED F_MUL(FIXED a, FIXED b) {
    int64_t tmp;
    tmp = ((int64_t)a) * b / (FIXED_F);
    // tmp = a / (FIXED_F) * b;
    return (int32_t) tmp;
}
static FIXED F_DIV(FIXED a, FIXED b) {
    int64_t tmp;
    // a = a / b * FIXED_F;
    tmp = ((int64_t)a) * (FIXED_F) / b;
    return (int32_t) tmp;
}
static int F_Rounded_UP(FIXED n) {
    if(n >= 0)
        return (n + (1 << 13)) / (FIXED_F);
    else 
        return (n - (1 << 13)) / (FIXED_F);
}

#endif