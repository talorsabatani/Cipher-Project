#include <cuda_runtime.h>
#include <helper_cuda.h>
#include "cipher.h"
#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#define NUM_OF_THREADS 1024  // maximum number of threads per block

// run on the kernel
__global__ void addCalculateKernel(char *cipher_text, int text_len, unsigned char *key, int key_len_as_byts, int* d_global_all_ascii) {
	int tid = threadIdx.x;
	int charId = tid;

	// shared memory is allocated between threads in a block
	__shared__ int shared_non_ascii_counter;
	__shared__ int maximum_non_ascii_allowed;
	//__shared__ unsigned int key_as_int;

	if (tid == 0) {
		shared_non_ascii_counter = 0; 
		maximum_non_ascii_allowed = text_len * MAX_PERCENTAGE_NON_PRINTABLE; // we are allowing some percentage of not-printable ascii
		if (maximum_non_ascii_allowed <= 0) {
			maximum_non_ascii_allowed = 1;
		}
		/*
		key_as_int = 0;
		for (int i = 0; i < key_len_as_byts; i++) {
			key_as_int *= 256;
			key_as_int += key[i];
		} */
	}

	// sync threads in the same block
	__syncthreads();

	while (charId < text_len) { // out of the borders of the text
		if (shared_non_ascii_counter < maximum_non_ascii_allowed) { // checks if the number of non-ascii allowed not reached to the maximum
			cipher_text[charId] ^=  key[charId % (key_len_as_byts)]; 

			//if isn't printable ascii characters		
			// 13 and 10 together are "\n". 32 to 126 are all the printable ascii characters.
			if (!(cipher_text[charId] == LINE_FEED || cipher_text[charId] == CARRIAGE_RETURN || (MIN_PRINTABLE <= cipher_text[charId] && cipher_text[charId] <= MAX_PRINTABLE))) {
				//if (key_as_int != 111) 	
				atomicAdd(&shared_non_ascii_counter, 1); // used only one block at a time
				//else
				//	printf("key %u, char id: %d, not good int: %u\n", key_as_int, charId, (unsigned char)cipher_text[charId]);
			}

		}

		charId += NUM_OF_THREADS;
	}

	// sync threads
	__syncthreads();
	
	if (tid == 0) {
		// checks if the number of non-ascii allowed reached to the maximum
		if (shared_non_ascii_counter > maximum_non_ascii_allowed) {
			*d_global_all_ascii = 0;
		}
	}

}

int cuda_calc_plain(char *cipher_text, int text_len, unsigned int key_as_int, int key_len_as_byts, char** plain_text) {

	char *d_temp_text = 0;
	unsigned char* d_temp_key = 0;
	int* d_global_all_ascii = 0;
	int cpu_all_ascii = 1;
	int num_block = 1;// text_len / NUM_OF_THREADS + 1;
	cudaError_t cudaStatus;
	int i = key_len_as_byts-1; 

	*plain_text = (char*)calloc(text_len, sizeof(char));
	if (!(*plain_text)) {
		printf("calloc for plain_text faliled.");
		return -1;
	}

	unsigned char* key = (unsigned char*)calloc(key_len_as_byts, sizeof(unsigned char));
	if (!key) {
		printf("calloc for key faliled.");
		return -1;
	}

	while (key_as_int > 0) {
		key[i] = (unsigned char) (key_as_int % 256); // get the first right byte into the key[i]
		key_as_int /= 256; // remove the first right byte
		i--;
	}		

	// Choose which GPU to run on
	cudaStatus = cudaSetDevice(0);

	// allocated space - create d_global_all_ascii for cuda
	cudaStatus = cudaMalloc((void**) &d_global_all_ascii, sizeof(int));
	if (cudaStatus != cudaSuccess) {
		printf("cudaMalloc all_ascii failed.");
		return -1;
	}

	//reset to 1 in all_ascii in gpu
	cudaStatus = cudaMemcpy(d_global_all_ascii, &cpu_all_ascii, sizeof(int), cudaMemcpyHostToDevice);
	if (cudaStatus != cudaSuccess) {
		printf("cudaMemcpy failed.");
		return -1;
	}


	// create d_temp_text for cuda
	cudaStatus = cudaMalloc((void**) &d_temp_text, text_len * sizeof(char));
	if (cudaStatus != cudaSuccess) {
		printf("cudaMalloc d_temp_text failed.");
		return -1;
	}

	//Copy cipher text from cpu to gpu
	cudaStatus = cudaMemcpy(d_temp_text, cipher_text, text_len * sizeof(char), cudaMemcpyHostToDevice);
	if (cudaStatus != cudaSuccess) {
		printf("cudaMemcpy failed.");
		return -1;
	}

	// create d_temp_key for cuda
	cudaStatus = cudaMalloc((void**) &d_temp_key, key_len_as_byts * sizeof(unsigned char));
	if (cudaStatus != cudaSuccess) {
		printf("cudaMalloc d_temp_key failed.");
		return -1;
	}

	//Copy key from cpu to gpu
	cudaStatus = cudaMemcpy(d_temp_key, key, key_len_as_byts * sizeof(unsigned char), cudaMemcpyHostToDevice);
	if (cudaStatus != cudaSuccess) {
		printf("cudaMemcpy failed.");
		return -1;
	}

	
	//Launch a kernel on the GPU with 1024 threads for every block
	// num_block = text_len / NUM_OF_THREADS +1
	// NUM_OF_THREADS = 1024
	addCalculateKernel<<<num_block, NUM_OF_THREADS>>>(d_temp_text, text_len, d_temp_key, key_len_as_byts, d_global_all_ascii);

	//Copy all_ascii from gpu to cpu
	cudaStatus = cudaMemcpy(&cpu_all_ascii, d_global_all_ascii, sizeof(int), cudaMemcpyDeviceToHost);
	if (cudaStatus != cudaSuccess) {
		printf("cudaMemcpy failed.");
		return -1;
	}

	if(cpu_all_ascii){
		//Copy plain text from gpu to cpu
		cudaStatus = cudaMemcpy(*plain_text, d_temp_text, text_len * sizeof(char), cudaMemcpyDeviceToHost);
		if (cudaStatus != cudaSuccess) {
			printf("cudaMemcpy failed.");
			return -1;
		}
	}

	// free memory
	cudaFree(d_temp_text);
	cudaFree(d_temp_key);
	cudaFree(d_global_all_ascii);
	free(key);

	
	// return the status that says if all is printable ascii (0-no 1-yes)
	return cpu_all_ascii;
}

