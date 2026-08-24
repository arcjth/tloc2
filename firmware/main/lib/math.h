#ifndef TLOC_MATH_H
#define TLOC_MATH_H

#define f64 double

// somehow it doesn't have it
#define bool char
#define true  1
#define false 0

typedef struct {
    f64 i, j;
} vec2;

typedef struct {
    f64  A[3][3];
    f64  b[3];
    f64  x[3];
    bool solved;
} linSys3;

void lin3_gaussian_solve(linSys3 *sys);

#endif
