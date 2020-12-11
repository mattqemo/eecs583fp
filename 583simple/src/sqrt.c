#include <stdio.h>
#include <math.h>
#include "../../fp.h"


// common expression elimination
typedef struct rect {
    int width, height;
} rect_t;

int getArea_PURE_(rect_t R) {
  return R.width * R.height;
}

double calcStuff_PURE_(rect_t* first, rect_t* second) {
  return sqrt(getArea_PURE_(*first)) / (sqrt(getArea_PURE_(*second)) + 1);
}

int main(int argc, char* argv[]) {
  // int vector [1, 2, 3] -> user passes index for a and index for b
  rect_t A = {5, 10};
  rect_t* ptr_a = &A;
  rect_t* ptr_b = &A;

  double result = calcStuff_PURE_(ptr_a, ptr_b);
  printf("result is %f\n", result);

  return 0;
}
