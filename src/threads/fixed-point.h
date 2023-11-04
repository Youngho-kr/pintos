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
#define FIXED_F (1<<FIXED_Q)

typedef int FIXED;
/* Initial Fixed-point is caused by divide 
   Fixed-point for integer can be dived by 1
*/
static FIXED F_INIT(int a, int b) {
    return (a * FIXED_F) / b;
}
static FIXED F_ADD(FIXED a, FIXED b) {
    return a + b;
}
static FIXED F_MUL(FIXED a, FIXED b) {
    return a * b / FIXED_F;
}
static FIXED F_DIV(FIXED a, FIXED b) {
    return a / b * FIXED_F;
}
static int F_Rounded_UP(FIXED n) {
    if(n >= 0)
        return (n + 1 << 13) / FIXED_F;
    else 
        return (n - 1 << 13 ) / FIXED_F;
}

#endif