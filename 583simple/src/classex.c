#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../../fp.h"

typedef struct Num {
    int* valPtr;
    int isInf;
} Num_t;

void ConstructNum(Num_t* self, int* valPtr_in) {
    self->valPtr = valPtr_in;
}

double fn_PURE_(int val) {
  double d = 3.7;
  for (int i = 0; i < 1000; ++i) {
    d = (int)((val + 2.0) / 10) % 7 + 1.0;
  }
  return d;
}

const int ARR_SIZE = 6;
int arr[ARR_SIZE] = {5, 8, 3, 5, 8, 3};

int GetRandIdxSometimes(int percent_likelihood_same, int orig) {
  return ((rand() % 100) < percent_likelihood_same) ? orig : (orig + 1) % ARR_SIZE;
}

int main(int argc, char* argv[]) {
  srand(time(NULL));
    // [1, 2, 3]: user passes in 2 indices
  int i1, i2, loopCount, percent_likelihood_same, shouldPrint;
  char c;
  sscanf(argv[1], "%d %c %d %c %d %c %d %c %d", &i1, &c, &i2, &c, &loopCount, &c, &percent_likelihood_same, &c, &shouldPrint);
  if (shouldPrint)
    fprintf(stderr, "percent likelihood: %d\n", percent_likelihood_same);
  if (i1 >= ARR_SIZE || i2 >= ARR_SIZE) return fprintf(stderr, "NO STOP\n");

  while (loopCount --> 0) {
    Num_t a;
    Num_t b;
    int i1rand = GetRandIdxSometimes(percent_likelihood_same, i1);
    int i2rand = i2;
    ConstructNum(&a, arr + i1rand);
    a.isInf = 0;
    ConstructNum(&b, arr + i2rand);
    double res_a = fn_PURE_(*a.valPtr);
    double res_b = fn_PURE_(*b.valPtr);

    printf("results are: %f %f \n", res_a, res_b);
  }

  return 0;
}

/*
TODO
idea to show: 2D grah of evolution runtime by changing optimpass threshold and
percent_likelihood_same used in GetRandIdxSometimes() here
*/
