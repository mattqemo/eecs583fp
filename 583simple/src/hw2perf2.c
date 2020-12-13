#include <stdio.h>
#include <stdlib.h>

int main() {
	double A[1000];
	double B[1000];
	double C[1000];
	int i, j;
	for(i = 0; i < 1000; i++){
		A[i] = i * 2.2398;
		B[i] = 0;
		C[i] = i * 1.3049;
	}
	srand(3);

	j = 5;
	for(i = 0; i < 1000000000; i++) {
  		double temp = (C[j] * 3.1415926 * 65.468) / 27.3;
  		double temp2 = temp * (temp / 46.354) * 6.51984;
  		double temp3 = temp2 * 25.13 * 51.016 / 32.186 * A[j];
  		B[i % 1000] = (35.48 * A[j] / 1.843) * (temp3 * 8.7805) / 5.405 * 2.79 + i;
  		if(i % 987654321 == 0) {
  			if(i % 2 == 0)
  				j = rand() % 1000;
  			else
  				j = 37;
  		}
	}

	for(i = 0; i < 1000; i++)
		printf("%f\n", B[i]);
	
	return 0;
}
