//#include "fixed-point.h"

#include <stdio.h>
// #include <assert.h>
#include <stdint.h>
#include <inttypes.h>

#define Q 14
int32_t f = 1 << Q;

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
fp32_t multiply_int(fp32_t x, int32_t n);
fp32_t divide_int(fp32_t x, int32_t n);

/* Takes an integer in normal form and converts it to p.q fixed point */
fp32_t convert_fp(int32_t n)
{
  return n * f;
}

/* Take a p.q fixed point number and converts it into an integer in normal form,
  rounding towards zero (e.g. down for positive numbers, up for negative) */
int32_t convert_integer_z(fp32_t x)
{
  return x / f;
}

/* Take a p.q fixed point number and converts it into an integer in normal form,
  rounding to nearest whole number*/
int32_t convert_integer_n(fp32_t x)
{
  if (x >= 0)
    return (x + f / 2) / f;
  return (x - f / 2) / f;
}

/* Adds two fp numbers and returns a fp number */
fp32_t add_fp(fp32_t x, fp32_t y)
{
  return x + y;
}

/* Subtracts fp number y from fp number x and returns
  a fp number */
fp32_t subtract_fp(fp32_t x, fp32_t y)
{
  return x - y;
}

/* Adds an fp to a normal int and returns a fp number */
fp32_t add_int(fp32_t x, int32_t n)
{
  return x + convert_fp(n);
}

/* Subtracts an int from a fp number and returns a fp 
  number */
fp32_t subtract_int(fp32_t x, int32_t n)
{
  return x - convert_fp(n);
}

/* Multiplies two fp numbers and returns a fp number */
fp32_t multiply_fp(fp32_t x, fp32_t y)
{
  return ((int64_t) x) * y / f;
}

/* Multiplies an fp number with an int and returns a 
  fp number */
fp32_t multiply_int(fp32_t x, int32_t n)
{
  return x * n;
}

/* Divides fp nuumber x by fp number y and returns a
  fp number */
fp32_t divide_fp(fp32_t x, fp32_t y)
{
  return ((int64_t) x) * f / y;
}

/* Divides an int from a fp */
fp32_t divide_int(fp32_t x, int32_t n)
{
  return x / n;
}

/* For testing implementation */
// int main(int argc, char *argv[])
// {
//   //Demonstrates that 1/2 = 0.5
//   //In this case 16384/32768 = 8192
//   int32_t a = 1;
//   int32_t b = 2;
//   fp32_t x = convert_fp(a);
//   fp32_t y = convert_fp(b);
//   fp32_t z = divide_fp(x,y);
  
//   /* show the current values */
//   printf("x = %d\n", x);
//   printf("y = %d\n", y);
//   printf("z = %d\n\n", z);

//   //Demonstates 256^2 = 65536

//   a = 256;
//   x = convert_fp(a);
//   fp32_t w = multiply_fp(x,x);
//   a = convert_integer_z(w);

//   printf("x = %d\n", x);
//   printf("w = %d\n", w);
//   printf("a = %d\n", a);

//   //Demonstates 65536 * 0.5 using w and z
//   //is equal to 65536 / 2  using w and 2.

//   assert(multiply_fp(w,z) == divide_int(w,2));

//   return 0;
// }






