#include <stdio.h>
#include <stdlib.h>

int main() {
	long long int A[1000];
	long long int B[1000];
	long long int C[1000];
	long long int D[1000];
	int i, j, k, x, y;
	for(i = 0; i < 1000; i++){
		A[i] = i;
		B[i] = i * 2;
		C[i] = i * 3;
		D[i] = 0;
	}
	srand(4);

	j = 5;
	k = 15;
	x = 0;
	y = 0;
	for(i = 0; i < 1000000000; i++) {	
		long long int temp = A[j] * 3 * 29 / 4 + 23;
		long long int temp2 = B[k] * 7 * 33 / 3 + 59;
		long long int temp3 = C[(temp + temp2) % 1000] + A[j] + B[k];
		D[i % 1000] = temp * 11 + temp2 * 13 + temp3 * 17 + i;
  		if(i < 100) {
  			for(x = 0; x < 1000000; x++){
  				long long int z = B[y] * 6 + C[y] / 2;
  				long long int z2 = 2 * A[z % 1000] + 4 * B[y];
  				long long int z3 = 6 * C[z2 % 1000] + A[y] * 3;
  				D[x % 100] = z + z2 + z3 * 2 + x; 
  				if(x < 10)
  					y = 59;
  			}
  			if(i == 0)
  				j = rand() % 1000;
  			else
  				k = 37;
  		}
	}

	for(i = 0; i < 1000; i++)
		printf("%lld\n", D[i]);
	
	return 0;
}
