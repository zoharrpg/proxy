
#include "csapp.h"

#define MAX_CACHE_SIZE (1024 * 1024)
#define MAX_OBJECT_SIZE (100 * 1024)

typedef struct Block {
    int lru_count;
    char key[MAXLINE];
    char value[MAX_OBJECT_SIZE];
    size_t value_length;
    struct Block *prev;
    struct Block *next;

} block_t;

block_t *search_evict_block();

block_t *block_init(char key[MAXLINE], char value[MAX_OBJECT_SIZE],
                    size_t length);

void remove_block(block_t *block);

void add_block(char key[MAXLINE], char value[MAX_OBJECT_SIZE], size_t length);

block_t *chceck_cache_repeat(char key[MAXLINE]);

// search cache
block_t *search_cache(char key[MAXLINE]);

void cache_init();

void cache_free();