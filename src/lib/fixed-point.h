#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdint.h>
#include <inttypes.h>

/* Where Q + P = 31 */
#define Q 14

/* So the programmer knows that their number is 
   in p.q fixed point form */
typedef int32_t fp32_t;

/* Conversion functions */
fp32_t convert_fp(int32_t n);
int32_t convert_integer_z(fp32_t x);
int32_t convert_integer_n(fp32_t x);

/* Arithmetic functions on 2 fp numbers */
fp32_t add_fp(fp32_t x, fp32_t y);
fp32_t subtract_fp(fp32_t x, fp32_t y);
fp32_t multiply_fp(fp32_t x, fp32_t y);
fp32_t divide_fp(fp32_t x, fp32_t y);

/*Aritmetic functions with 1 fp number */
fp32_t add_int(fp32_t x, int32_t n);
fp32_t subtract_int(fp32_t x, int32_t n);
fp32_t multiply_int(fp32_t x, int32_t n;
fp32_t divide_int(fp32_t x, int32_t n);

/* To be removed once integrated into pintos/
   proper tests are added to src/tests */
//int main(int argc, char *argv[]);

#endif /* threads/fixed-point */