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
  int y = x;
  int* yPtr = &y;
  for (int i = 0; i < 3; ++i) {
    int z = *yPtr + 1;
    x += z;
  }
  randomfcn();
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
