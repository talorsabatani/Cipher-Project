build:
	mpicxx -fopenmp -c cipher.c -o cipher.o
	nvcc -I./inc -c cuda_encode.cu -o cuda_encode.o
	mpicxx -fopenmp -o mpiCudaOpemMP  cipher.o cuda_encode.o  /usr/local/cuda-9.1/lib64/libcudart_static.a  -ldl -lrt

clean:
	rm -f *.o cipher

run:
	mpiexec -np 2 ./mpiCudaOpemMP  1 -i input.txt -w words.txt


runwithoutwords:
	mpiexec -np 2 ./mpiCudaOpemMP  1 -i input.txt


runinput:
	mpiexec -np 1 ./mpiCudaOpemMP  1 -i input.txt -w words.txt

runhello:
	mpiexec -np 1 ./mpiCudaOpemMP  1 -i helloWorld.txt -w words.txt

runjoke:
	mpiexec -np 1 ./mpiCudaOpemMP  1 -i lawyerJoke.txt -w words.txt

runaddress:
	mpiexec -np 1 ./mpiCudaOpemMP  2 -i gettysburgAddress.txt -w words.txt

runmovie:
	mpiexec -np 1 ./mpiCudaOpemMP  4 -i movie_quotes.txt -w words.txt


runproc1key1:
	mpiexec -np 1 ./mpiCudaOpemMP  1 -i input.txt -w words.txt

runproc1key2:
	mpiexec -np 1 ./mpiCudaOpemMP  2 -i input.txt -w words.txt

runproc1key3:
	mpiexec -np 1 ./mpiCudaOpemMP  3 -i input.txt -w words.txt

runproc1key4:
	mpiexec -np 1 ./mpiCudaOpemMP  4 -i input.txt -w words.txt


runproc2key1:
	mpiexec -np 2 ./mpiCudaOpemMP  1 -i input.txt -w words.txt

runproc2key2:
	mpiexec -np 2 ./mpiCudaOpemMP  2 -i input.txt -w words.txt

runproc2key3:
	mpiexec -np 2 ./mpiCudaOpemMP  3 -i input.txt -w words.txt

runproc2key4:
	mpiexec -np 2 ./mpiCudaOpemMP  4 -i input.txt -w words.txt


runproc3key1:
	mpiexec -np 3 ./mpiCudaOpemMP  1 -i input.txt -w words.txt

runproc3key2:
	mpiexec -np 3 ./mpiCudaOpemMP  2 -i input.txt -w words.txt

runproc3key3:
	mpiexec -np 3 ./mpiCudaOpemMP  3 -i input.txt -w words.txt

runproc3key4:
	mpiexec -np 3 ./mpiCudaOpemMP  4 -i input.txt -w words.txt

