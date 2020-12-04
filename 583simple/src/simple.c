#include <stdio.h>

#include "../../fp.h"

void randomfcn() {
  int a = 5;
  a = a;
  int b = a;
}

int main()
{
  int x = 10;
  int* xptr = &x;
  int** xptrptr = &xptr;
  int y = 15;
  int* yptr = &y;
  *xptrptr = yptr;
  int arr[2];
  arr[0] = 0;
  // int x = 10;
  // int y = x;
  // int* yPtr = &y;
  // int arr[8] = {0};
  // for (int i = 0; i < 8; ++i) {
  //   int z = *yPtr + 1;
  //   x += z;
  //   arr[i] = z + y;
  // }
  // randomfcn();
  // temp(&x);

  // int in[1000];
  // int i,j;
  // FILE* myfile;

  // for (i = 0; i < 1000; i++)
  // {
  //   in[i] = 0;
  // }

  // for (j = 100; j < 1000; j++)
  // {
  //  in[j]+= 10;
  // }


  // for (i = 0; i< 1000; i++)
  //   fprintf(stdout,"%d\n", in[i]);

  return 1;
}
