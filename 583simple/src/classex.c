#include <stdio.h>
#include <stdlib.h>
#include "../../fp.h"

typedef struct Num {
    int* valPtr;
    int isInf;
} Num_t;

void ConstructNum(Num_t* self, int* valPtr_in) {
    self->valPtr = valPtr_in;
}

// double fn_PURE_(Num_t* n) {
//     return *n->valPtr + 2.0;
// }
double fn_PURE_(int* val) {
    return *val + 2.0;
}

int main(int argc, char* argv[]) {
  int val = strtol(argv[1], NULL, 10);
  int* valPtr = (int *)malloc(sizeof(int));
  *valPtr = val;

  Num_t* a = (Num_t *)malloc(sizeof(Num_t));
  Num_t* b = (Num_t *)malloc(sizeof(Num_t));
  ConstructNum(a, valPtr);
  a->isInf = 0;
  ConstructNum(b, valPtr);

  double res_a = fn_PURE_(a->valPtr);
  double res_b = fn_PURE_(b->valPtr);

  printf("results are: %f %f \n", res_a, res_b);

  return 0;
}
