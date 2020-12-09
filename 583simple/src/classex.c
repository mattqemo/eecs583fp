#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../../fp.h"

struct Num {
    int* valPtr;
    bool isInf;
};

void ConstructNum(Num* self, int* valPtr_in) {
    self->valPtr = valPtr_in;
}

double fn_PURE_(Num* n) {
    return sqrt(*n->valPtr);
}

int main(int argc, char* argv[]) {
  int val = strtol(argv[1], NULL, 10);
  int* valPtr = (int *)malloc(sizeof(int));
  *valPtr = val;

  Num* a = (Num *)malloc(sizeof(Num));
  Num* b = (Num *)malloc(sizeof(Num));
  ConstructNum(a, valPtr);
  a->isInf = false;
  ConstructNum(b, valPtr);

  double res_a = fn(a);
  double res_b = fn(b);

  printf("results are: %f %f \n", res_a, res_b);

  return 0;
}
