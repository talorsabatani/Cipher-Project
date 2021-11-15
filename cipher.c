#include <unistd.h>

#include <mpi.h>
#include <stdio.h>
#include <omp.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include "cipher.h"

// the words file stored in this path 
#define WORDS_FILE_PATH "/usr/share/dict/words" 

#define START_SIZE 512
#define EXTEND_SIZE 32
#define MAX_KEY_SIZE 4

#define CUDA_PRECENTAGE 50

#define NUMBER_OF_BITS_IN_KEY 16

#define hashsize(n) ((ub4)1<<(n))  // 1 is left-shifted by n
#define hashmask(n) (hashsize(n)-1) // return n units. for example (hashsize(n = 3)-1) --> return 111

#define mix(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}


enum ranks {
	MASTER, SLAVE, MAX_PROCS};

//int cuda_percentage = 90;
char* computerName = NULL;

int main(int argc, char *argv[]) {
	int num_processes, pid, text_len, part_size;
	char *plain_text = NULL;
	char *cipher_text = NULL;
	int key_len_as_byts;
	node_t* node_map = NULL;
	FILE *words = NULL;
	double totalTime = 0.0;
	int bufferLength = MPI_MAX_PROCESSOR_NAME; // returns the length of the processor name - The buffer for "name" must be at least MPI_MAX_PROCESSOR_NAME characters in size

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &pid); // find out the process rank
	MPI_Comm_size(MPI_COMM_WORLD, &num_processes); // find out the number of processes
	
	// get computer name
	computerName = (char*)malloc(bufferLength*sizeof(char));
	MPI_Get_processor_name(computerName, &bufferLength);

	if (pid == MASTER) {
		FILE *input = NULL;
		//FILE *output = NULL;
		int i;

		// open input file from the buffer for reading
		input = open_input_file(argc, argv);
		if (input == NULL) {
			return 0;
		}

		//Get text
		cipher_text = readStringFromFile(input, START_SIZE, &text_len);
		if (!cipher_text) {
			fprintf(stderr, "Error reading string\n");
			exit(0);
		}

		fclose(input);

		// get key length
		key_len_as_byts = atoi(argv[1]);

		// maximum length is 4
		if (key_len_as_byts > MAX_KEY_SIZE)
			key_len_as_byts = MAX_KEY_SIZE;

		printf("\n############################ Before Run ############################\n\n");
		printf("Cipher Text\n");
		printf("-----------\n");
		printf("%s\n\n", cipher_text);

		printf("Text Prameters\n");
		printf("--------------\n");
		printf("Text Length: %d\n\n", text_len);

		printf("Program Settings\n");
		printf("----------------\n");
		printf("Number of Processes: %d\n", num_processes);
		printf("Key Length in Bytes: %d\n", key_len_as_byts);
		printf("Maximum Percentage of Non-Printable Characters: %.2f\n", MAX_PERCENTAGE_NON_PRINTABLE);
		printf("Minimum Percentage of Words Found in Dictionary: %.2f\n", MIN_PERCENTAGE_IN_DICTIONARY);
		printf("Cpu Percentage: %d%%\n", 100 - CUDA_PRECENTAGE);		
		printf("Cuda Percentage: %d%%\n\n", CUDA_PRECENTAGE);
		
	}

	//open words file for reading 
	words = open_words_file(argc, argv);
	// if we don't have a given words file from the buffer, we will use a list of dictionary words
	if (!words) {
		words = fopen(WORDS_FILE_PATH, "r");
		if (!words) {
			fprintf(stderr, "Error opening file\n");
			exit(0);
		}
	}

	// get a words map
	node_map = readWordsFromFile(words);
	if (!node_map) {
		fprintf(stderr, "Error building map\n");
		exit(0);
	}

	fclose(words);
	
	/* send the key_len_as_byts, 
		the text_len
		and the cipher_text 
		from the master process to all other processes of the communicator */
	MPI_Bcast(&key_len_as_byts, 1, MPI_INT, MASTER, MPI_COMM_WORLD);
	MPI_Bcast(&text_len, 1, MPI_INT, MASTER, MPI_COMM_WORLD);

	if(pid != MASTER){
		cipher_text = (char*)malloc(text_len*sizeof(char));
	}

	MPI_Bcast(cipher_text, text_len, MPI_CHAR, MASTER, MPI_COMM_WORLD);
	
	totalTime -= MPI_Wtime();  // returns an elapsed time on the calling processor
	encode_cipher_text(pid, num_processes, cipher_text, text_len, key_len_as_byts, node_map); //find the plain text 
	totalTime += MPI_Wtime();

	// print and end program
	if (pid == MASTER)
		printf("Calculation Time: %lf\n", totalTime);

	free(cipher_text);
	free_node_map(node_map);
	MPI_Finalize();
	return 0;
}

void encode_cipher_text(int pid, int num_processes, char* cipher_text, int text_len, int key_len_as_byts, node_t* node_map){
	unsigned long int total_keys, process_keys, rest_of_keys;
	int key_counter = 0;
	int found = 0, is_other_process_found = 0, *other_process_found_arr = NULL;
	omp_lock_t counter_lock; // lock variable

	total_keys = pow(2, key_len_as_byts*8); // total key options 2^?bits
	process_keys = total_keys/num_processes; // number of keys for each process
	rest_of_keys = total_keys%num_processes; // rest of keys
	
	// initialize lock variable 
	omp_init_lock(&counter_lock);

	if (pid == MASTER) {
		other_process_found_arr = (int*)calloc(num_processes*2+1, sizeof(int)); // initialization found array
	}

	// printf("\nim here :)\n");
	// printf("\nprocess keys %lu, rest_of_keys %lu\n", process_keys, rest_of_keys);

#pragma omp parallel for
	// runs on all the keys in the pid process 
	for (unsigned long int i = pid*process_keys; i < pid*process_keys + process_keys; i++) { 
		unsigned int key_as_int;
		int result_code = -1; //-1: intial, 0: Illegal, more than 2% non-ascii, 1: ok for printing
		char* plain_text;
		int tid = omp_get_thread_num();
		int total_threads = omp_get_num_threads();

		if (!found || (!is_other_process_found && tid == 0)) { // :(
			key_as_int = (unsigned int)i;
			
			// divide the work between CPU and Cuda by cuda_percentage
			if (((tid+1)*100)/total_threads > CUDA_PRECENTAGE) { 
				// cpu calculation -return 1 if we didn't exceed the maximum non-ascii allowed else return 0
				result_code = cpu_calc_plain(cipher_text, text_len, key_as_int, key_len_as_byts, &plain_text); 
			} else {
				// cuda calculation
				result_code = cuda_calc_plain(cipher_text, text_len, key_as_int, key_len_as_byts, &plain_text); 
			}

			if (result_code == 1) { // 1: ok for printing
				//printf("\nkey_as_int: %u ,plain_text: %s \n", key_as_int, plain_text);
				if (node_map != NULL) {	
					// checks the match of the words in the plain_text on the node_map
					if (isThisAGoodDecoding(node_map, plain_text)) { 
						found = 1;
						printf("\n############################ In Run ############################\n\n");
						printf("Plain Text Found\n");
						printf("-----------------\n");
						printf("%s\n\n", plain_text);
						printf("Key Found: %u\n\n", key_as_int);
					}
				}
			}

			// this block is for us to stop the other processes if we find the key
			//////////////////////////////////////////////////////////////////////
			if (tid == 0) {
				MPI_Gather(&found, 1, MPI_INT, other_process_found_arr, num_processes, MPI_INT, MASTER, MPI_COMM_WORLD); // get an array of findings for all the processes
				if (pid == MASTER) {
					for (int j = 0; j < num_processes; j++) {
						if (other_process_found_arr[j] == 1){
							is_other_process_found = 1;
							break;
						}
					}
				}
			}

			if (tid == 0) {
				MPI_Bcast(&is_other_process_found, 1, MPI_INT, MASTER, MPI_COMM_WORLD);	
			}

			if (is_other_process_found == 1) {
				found = 1;
			}
			//////////////////////////////////////////////////////////////////////

			free(plain_text);

			omp_set_lock(&counter_lock); // wait until the lock is available, then set it. No other thread can set the lock until it's released 
			key_counter++;
			omp_unset_lock(&counter_lock); // release the lock 
		}
	}

	if (pid == MASTER && rest_of_keys > 0) { //  The MASTER process calculates the remaining keys
#pragma omp parallel for
		for (unsigned long int i = total_keys-rest_of_keys; i < total_keys; i++) {
			unsigned int key_as_int;
			int result_code = -1;
			char* plain_text;
			int tid = omp_get_thread_num();
			int total_threads = omp_get_num_threads();

			if (!found) {
				key_as_int = (unsigned int)i;
				//// decoding the text
				if (((tid+1)*100)/total_threads > CUDA_PRECENTAGE) {
					result_code = cpu_calc_plain(cipher_text, text_len, key_as_int, key_len_as_byts, &plain_text);
				} else {
					result_code = cuda_calc_plain(cipher_text, text_len, key_as_int, key_len_as_byts, &plain_text);
				}
							
				if (result_code == 1) { // 1: ok for printing
					//printf("\nkey_as_int: %u ,plain_text: %s \n", key_as_int, plain_text);
					if (node_map != NULL) {					
						if (isThisAGoodDecoding(node_map, plain_text)) {
							found = 1;
							printf("\n############################ In Run ############################\n\n");
							printf("Plain Text Found\n");
							printf("-----------------\n");
							printf("%s\n\n", plain_text);
							printf("Key Found: %u\n\n", key_as_int);
						}
					}
				}

				free(plain_text);

				omp_set_lock(&counter_lock); // wait until the lock is available, then set it. No other thread can set the lock until it's released
				key_counter++;
				omp_unset_lock(&counter_lock); // release the lock
			}
		}
	}

	if (pid == MASTER) {
		printf("\n############################ After Run ############################\n\n");
	}

	printf("Computer: %s\tPId: %d\tNumber of Keys Searched: %d\n", computerName, pid, key_counter);

	omp_destroy_lock(&counter_lock);

	if (pid == MASTER) {
		free(other_process_found_arr);
	}
}

// return 1 - if we didn't exceed the maximum non-ascii allowed else return 0
int cpu_calc_plain(char *cipher_text, int text_len, unsigned int key_as_int, int key_len_as_byts, char** plain_text) {
	int non_ascii_counter = 0;
	int maximum_non_ascii_allowed = text_len*MAX_PERCENTAGE_NON_PRINTABLE;
	int j = key_len_as_byts-1; 

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
		key[j] = (unsigned char) (key_as_int % 256); // get the first right byte into the key[j]
		key_as_int /= 256; // remove the first right byte
		j--;
	}		


	for (int i = 0; i < text_len; i++) {
		(*plain_text)[i] = cipher_text[i] ^ key[i%key_len_as_byts];
		
		//if isn't printable ascii characters		
		// 13 and 10 together are "\n". 32 to 126 are all the printable ascii characters.
		if (!((*plain_text)[i] == LINE_FEED || (*plain_text)[i] == CARRIAGE_RETURN || (MIN_PRINTABLE <= (*plain_text)[i] && (*plain_text)[i] <= MAX_PRINTABLE))) {
			non_ascii_counter += 1; 
			// checks if the number of non-ascii allowed reached to the maximum
			if (non_ascii_counter > maximum_non_ascii_allowed) { 
				free(key);
				return 0;
			}
		}
	}

	free(key);
	return 1; // return 1 - if we didn't exceed the maximum non-ascii allowed
}

// checks the match of the words in the plain_text on the node_map
int isThisAGoodDecoding(node_t* node_map, char* plain_text) {
	const char s[9] = " \n.,\"?:-";
	char *token;
	char* temp_text = NULL;
	int word_len, key;
	int found_counter = 0, word_counter = 0;
	int minimum_words_in_dictionary;
	node_t* node = NULL;

	temp_text = (char*)malloc(strlen(plain_text)*sizeof(char));
	strcpy(temp_text, plain_text);

	/* get the first token */
	token = strtok(temp_text, s);

	/* walk through other tokens */
	while( token != NULL ) {
		word_counter++;
		strlwrt((unsigned char**)&token);
		word_len = strlen(token);
		if (word_len > 30) { // I guess 30 is the maximum word length that can be
			free(temp_text);			
			return 0;
		}
		key = hash((unsigned char*)token, word_len, 0)&hashmask(NUMBER_OF_BITS_IN_KEY); // 32bits&16bits = 16bits. max key: 2^16-1

		node = &node_map[key];
		if (node->word) { // there is a word in the existing index
			do {
				if (strcmp(node->word, token) == 0) {  // if we found the token in the node_map
					found_counter++;
					//printf( "hh-> %s %d %d\n", token, found_counter, word_counter);
					break;
				}
				node = node->next;
			} while (node);
		}
		token = strtok(NULL, s);
	}
	// the minimum number of words allowed to be found in the dictionary
	minimum_words_in_dictionary = word_counter*MIN_PERCENTAGE_IN_DICTIONARY; 
	if (minimum_words_in_dictionary <= 0) {  
		minimum_words_in_dictionary = 1; 
	}
	// more than the minimum percentage for words found in dictionary
	if (found_counter > minimum_words_in_dictionary) { 
		free(temp_text);
		return 1; 
	} else {  
		free(temp_text);
		return 0;  
	}
}

// convert string to lower case letters
void strlwrt(unsigned char **str) { 
	unsigned char* next = *str;

	while (*next != '\0') {
		*next = tolower(*next);
		next++; 
	}

}

void binaryStringToBinary(char *string, size_t num_bytes)
{
    int i,byte;
    unsigned char binary_key[MAX_KEY_SIZE];
    for(byte = 0;byte<num_bytes;byte++)
    {
        binary_key[byte] = 0;
        for(i=0;i<8;i++)
        {
            binary_key[byte] = binary_key[byte] << 1;
            binary_key[byte] |= string[byte*8 + i] == '1' ? 1 : 0;  
        }
    }
    memcpy(string,binary_key,num_bytes);
}

// read the string from the file. The function returns the string
char* readStringFromFile(FILE *fp, int allocated_size, int *input_length) {
	char *string, *input_str;
	int ch;

	*input_length = 0;

	string = (char*) realloc(NULL, sizeof(char) * allocated_size);
	if (!string)
		return string;
	while (EOF != (ch = fgetc(fp))) {  // if it's not the end of the file, gets the next character
		if (ch == EOF)
			break;
		string[*input_length] = ch;  // adds the character to the end of string
		*input_length += 1;
		if (*input_length == allocated_size) {  // if we got to the allocated size increase the string size
			string = (char*) realloc(string, sizeof(char) * (allocated_size += EXTEND_SIZE));
			if (!string)
				return string;
		}
	}
	input_str = (char*) realloc(string, sizeof(char) * (*input_length)); // copies input_length to input_string
	return input_str;
}


//  extracts the words from the file and puts them in a word map
// return a node map
node_t* readWordsFromFile(FILE *fp) {
	unsigned char *word = NULL; 
	node_t* node_map = NULL, * node = NULL;
	int words_counter = 0;
    	char x[1024];
	int word_len;
	int key;

	node_map = (node_t*)calloc(pow(2, NUMBER_OF_BITS_IN_KEY), sizeof(node_t)); // I choose to build a hash table with 2^16=65,536 rows. In the words file we have a maximum of 99,171 words.
	
	while (fscanf(fp, " %1023s", x) == 1) { // scan a word   
		// puts(x);
		word_len = strlen(x);
		// printf("%d\n", word_len);
		word = (unsigned char*)malloc((word_len+1)*sizeof(unsigned char));
		strcpy((char*) word, x);
		strlwrt(&word); // str to lowercase
		key = hash(word, word_len, 0)&hashmask(NUMBER_OF_BITS_IN_KEY); // 32bits&16bits = 16bits. max key: 2^16-1
		node = &node_map[key];
		if (node->word) { // the node isn't empty (there is a word)
			node_t* new_node = (node_t*)malloc(sizeof(node_t));  // linked list
			new_node->word = (char*)word;
			new_node->next = node->next;
			node->next = new_node;
		} else {  // the node is empty
			node->word = (char*)word;  // inserts a word into the node
		}
	}

	return node_map;
}

// open input file from the buffer for reading
FILE* open_input_file(int argc, char *argv[]) {
 
	FILE *cipher;
	cipher = stdin; // standard input. It takes text as input.
	int i;
	for (i = 2; i < argc; i++) {
		// the input file name will appear after the words "-input" or "-i"
		if (strcmp(argv[i], "-input") == 0 || strcmp(argv[i], "-i") == 0) {  
			i++;
			cipher = fopen(argv[i], "r");
			if (!cipher) {
				fprintf(stderr, "Error opening file\n");
				return NULL;
			}
			continue;
		}
	}

	return cipher;
}

//open words file for reading 
FILE* open_words_file(int argc, char *argv[]) {

	FILE *dict;
	dict = NULL;
	int i;
	for (i = 2; i < argc; i++) {
		if (strcmp(argv[i], "-words") == 0 || strcmp(argv[i], "-w") == 0) {
			i++;
			dict = fopen(argv[i], "r");
			if (!dict) {
				fprintf(stderr, "Error opening file\n");
				return NULL;
			}
			continue;
		}
	}

	return dict;
}

ub4 hash(
// typedef unsigned long int  ub4; // 4 bytes-32 bits 
register ub1 *k,        /* the key */
register ub4  length,   /* the length of the key */
register ub4  initval  /* the previous hash, or an arbitrary value */
)
{
   register ub4 a,b,c,len; 

   /* Set up the internal state */
   len = length;
   a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
   c = initval;         /* the previous hash value */

   /*---------------------------------------- handle most of the key */
   while (len >= 12)
   {
      a += (k[0] +((ub4)k[1]<<8) +((ub4)k[2]<<16) +((ub4)k[3]<<24));
      b += (k[4] +((ub4)k[5]<<8) +((ub4)k[6]<<16) +((ub4)k[7]<<24));
      c += (k[8] +((ub4)k[9]<<8) +((ub4)k[10]<<16)+((ub4)k[11]<<24));
      mix(a,b,c);
      k += 12; len -= 12;
   }

   /*------------------------------------- handle the last 11 bytes */
   c += length;
   switch(len)              /* all the case statements fall through */
   {
   case 11: c+=((ub4)k[10]<<24);
   case 10: c+=((ub4)k[9]<<16);
   case 9 : c+=((ub4)k[8]<<8);
      /* the first byte of c is reserved for the length */
   case 8 : b+=((ub4)k[7]<<24);
   case 7 : b+=((ub4)k[6]<<16);
   case 6 : b+=((ub4)k[5]<<8);
   case 5 : b+=k[4];
   case 4 : a+=((ub4)k[3]<<24);
   case 3 : a+=((ub4)k[2]<<16);
   case 2 : a+=((ub4)k[1]<<8);
   case 1 : a+=k[0];
     /* case 0: nothing left to add */
   }
   mix(a,b,c);
   /*-------------------------------------------- report the result */
   return c; // return c (32 bits)
}

void free_node_map(node_t* node_map) {
	int size = pow(2, NUMBER_OF_BITS_IN_KEY);
	node_t* next = NULL, *next_next = NULL;

	for (int i = 0; i < size; i++) {
		if(node_map[i].word != NULL){
			free(node_map[i].word);
			next = node_map[i].next;
			while (next) {
				next_next = next->next;
				free(next->word);
				free(next);
				next = next_next;
			}
		}
	}
	free(node_map);
}





