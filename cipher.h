#define MIN_PERCENTAGE_IN_DICTIONARY 0.75
#define MAX_PERCENTAGE_NON_PRINTABLE 0.02

// ASCII character
#define LINE_FEED 10  // \n = new line
#define CARRIAGE_RETURN 13 // \r - reset a device's position to the beginning of a line of text
#define MIN_PRINTABLE 32 // minimum optional character
#define MAX_PRINTABLE 126 // maximum optional character

typedef  unsigned long int  ub4;   /* unsigned 4-byte quantities */
typedef  unsigned  char ub1;   /* unsigned 1-byte quantities */
typedef struct node {
	char* word;
	struct node* next;
} node_t;

char* readStringFromFile(FILE *fp, int allocated_size, int *input_length) ;
FILE* open_input_file(int argc, char *argv[]) ;
FILE* open_words_file(int argc, char *argv[]) ;
int cuda_calc_plain(char *cipher_text, int text_len, unsigned int key_as_int, int key_len_as_byts, char** plain_text);
int cpu_calc_plain(char *cipher_text, int text_len, unsigned int key_as_int, int key_len_as_byts, char** plain_text);
void encode_cipher_text(int pid, int num_processes, char* cipher_text, int text_len, int key_len_as_byts, node_t* node_map);
node_t* readWordsFromFile(FILE *fp);
int isThisAGoodDecoding(node_t *node_map, char* plain_text);
void strlwrt(unsigned char **str);
void binaryStringToBinary(char *string, size_t num_bytes);
void free_node_map(node_t* node_map);
ub4 hash(
register ub1 *k,        /* the key */
register ub4  length,   /* the length of the key */
register ub4  initval  /* the previous hash, or an arbitrary value */
);




;
