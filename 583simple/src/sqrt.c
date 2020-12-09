#include <stdio.h>
#include <math.h>
#include "../../fp.h"


// common expression elimination
struct rect {
    int width, height;

    int getArea_PURE_(){
        return width * height;
    }
};

double calcStuff_PURE_(rect* first, rect* second) {
  return sqrt(first->getArea_PURE_()) / (sqrt(second->getArea_PURE_()) + 1);
}

int main(int argc, char* argv[]) {
  // int vector [1, 2, 3] -> user passes index for a and index for b
  rect A = {5, 10};
  rect* ptr_a = &A;
  rect* ptr_b = &A;

  double result = calcStuff_PURE_(ptr_a, ptr_b);
  printf("result is %f\n", result);

  return 0;
}
