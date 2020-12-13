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
double fn_PURE_(int val) {
  fprintf(stderr, "fn_PURE_\n");
  double d = 3.7;
  for (int i = 0; i < 1000; ++i) {
    d = (int)((val + 2.0) / 10) % 7 + 1.0;
  }
  return d;
}

const int ARR_SIZE = 6;
int arr[ARR_SIZE] = {5, 8, 3, 5, 8, 3};

int main(int argc, char* argv[]) {
    // [1, 2, 3]: user passes in 2 indices
  int i1, i2, loopCount;
  char c;
  sscanf(argv[1], "%d %c %d %c %d", &i1, &c, &i2, &c, &loopCount);
  fprintf(stderr, "Running with i1/i2 %d %d\n", i1, i2);
  if (i1 >= ARR_SIZE || i2 >= ARR_SIZE) return fprintf(stderr, "NO STOP\n");

  // while (loopCount --> 0) {
  //   Num_t a;
  //   Num_t b;
  //   ConstructNum(&a, arr + i1);
  //   a.isInf = 0;
  //   ConstructNum(&b, arr + i2);
  //   double res_a = fn_PURE_(a.valPtr);
  //   double res_b = fn_PURE_(b.valPtr);

  //   printf("results are: %f %f \n", res_a, res_b);
  // }

  Num_t a;
  Num_t b;
  int* i1_ptr = &arr[i1];
  int* i2_ptr = &arr[i2];
  ConstructNum(&a, i1_ptr);
  a.isInf = 0;
  ConstructNum(&b, i2_ptr);
  // int** avpp = &a.valPtr;
  // int** bvpp = &b.valPtr;
  // double res_a = fn_PURE_(*avpp);
  // double res_b = fn_PURE_(*bvpp);
  double res_a = fn_PURE_(*a.valPtr);
  double res_b = fn_PURE_(*b.valPtr);

  return 0;
}
