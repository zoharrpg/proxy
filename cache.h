
# include "csapp.h"

#define MAX_CACHE_SIZE (1024 * 1024)
#define MAX_OBJECT_SIZE (100 * 1024)
#define MAX_BLOCK_COUNT (MAX_CACHE_SIZE/MAX_OBJECT_SIZE)

typedef struct Block{
    int lru_count;
    char key[MAXLINE];
    char value[MAX_OBJECT_SIZE];
    size_t value_length;
    struct Block *prev;
    struct Block *next;
    
}block_t;

block_t *search_evict_block();

block_t* block_init(char key[MAXLINE],char value[MAX_OBJECT_SIZE],size_t length);

void remove_block(block_t* block);


void add_block(char key[MAXLINE],char value[MAX_OBJECT_SIZE],size_t length);

// search cache
block_t* search_cache(char key[MAXLINE]);
