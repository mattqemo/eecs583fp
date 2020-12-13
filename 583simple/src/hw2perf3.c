#include <stdio.h>
#include <stdlib.h>

int main() {
	long long int A[1000];
	long long int B[1000];
	long long int C[1000];
	long long int D[1000];
	int i, j, k;
	for(i = 0; i < 1000; i++){
		A[i] = i;
		B[i] = i * 2;
		C[i] = i * 3;
		D[i] = 0;
	}
	srand(4);

	j = 5;
	k = 15;
	for(i = 0; i < 1000000000; i++) {	
		long long int temp = A[j] * 3 * 29 / 4 + 23;
		long long int temp2 = B[k] * 7 * 33 / 3 + 59;
		long long int temp3 = C[(temp + temp2) % 1000] + A[j] + B[k];
		D[i % 1000] = temp * 11 + temp2 * 13 + temp3 * 17 + i;
  		if(i < 3) {
  			if(i == 0)
  				j = rand() % 1000;
  			else if (i == 1)
  				k = 37;
  			else{
  				j = 25;
  				k = 52;
  			}
  		}
	}

	for(i = 0; i < 1000; i++)
		printf("%lld\n", D[i]);
	
	return 0;
}
