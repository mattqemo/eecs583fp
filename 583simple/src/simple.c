#include <stdio.h>

#include "../../fp.h"

int main()
{
  int x = 10;
  int y = x;
  int* yPtr = &y;
  const char* str = "blah1\n";
  printf(str);
  printf("blah2\n");
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
