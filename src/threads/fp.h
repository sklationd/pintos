#include <stdio.h>
#define fadd(X,Y) ((X)+(Y))
#define fsub(X,Y) ((X)-(Y))
#define fmul(X,Y) ((((int64_t) (X)) * (Y))/(1<<14))
#define fdiv(X,Y) ((((int64_t) (X)) * (1<<14))/(Y))
#define itof(X)   ((X)<<14)
#define pftoi(X)  (((X)+(1<<13))>>14)
#define nftoi(X)  (((X)-(1<<13))>>14)
#define ftoi(X) ((X) >= 0 ? pftoi(X) : nftoi(X))