/**
 * @file cache.h
 * @brief Definitions and interfaces for cache.c
 */

#include "csapp.h"
#define MAX_CACHE_SIZE (1024 * 1024)
#define MAX_OBJECT_SIZE (100 * 1024)

/*doubly linked-list block structure in the cache*/
typedef struct Block {
    int lru_count;               /*LRU count*/
    char key[MAXLINE];           /*store the uri as the ky*/
    char value[MAX_OBJECT_SIZE]; /*Web object*/
    size_t value_length;         /*length of the web object*/
    struct Block *prev;          /*previous pointer*/
    struct Block *next;          /*next pointer*/

} block_t;

/**
 * The function searches for the evict block with the minimum LRU count in a
 * linked list and returns it.
 *
 * @return a pointer to a block_t structure.
 */
block_t *search_evict_block();

/**
 * The function initializes a block with a given key, value, length, and update
 * LRU count.
 *
 * @param key The "key" parameter is a character array that represents the key
 * associated with the block. It has a maximum length of MAXLINE.
 * @param value The "value" parameter in the "block_init" function is a
 * character array that represents the value associated with a key in a block.
 * It has a maximum size of MAX_OBJECT_SIZE.
 * @param length The "length" parameter represents the length of the web object
 * being stored in the block. It is of type "size_t", which is an unsigned
 * integer type used for representing sizes and counts.
 *
 * @return a pointer to a block_t structure.
 */
block_t *block_init(char key[MAXLINE], char value[MAX_OBJECT_SIZE],
                    size_t length);

/**
 * The function removes a block from a linked list and updates the cache size.
 *
 * @param block The `block` parameter is a pointer to a `block_t` structure.
 *
 */
void remove_block(block_t *block);

/**
 * The function `add_block` adds a new block to a cache, ensuring that the cache
 * does not exceed its maximum size.
 *
 * @param key A character array representing the key of the block to be added to
 * the cache. The key is used to identify the block. use uri as the key
 * @param value The `value` parameter in the `add_block` function is a character
 * array that represents the value associated with the given `key`. It has a
 * maximum size of `MAX_OBJECT_SIZE`.
 * @param length The `length` parameter represents the length of the valid web
 * object to be stored in the cache block.
 *
 * @return The function does not explicitly return a value.
 */
void add_block(char key[MAXLINE], char value[MAX_OBJECT_SIZE], size_t length);

/**
 * The function checks if a given key exists in a cache and returns the
 * corresponding block if found, otherwise returns NULL.
 *
 * @param key The parameter "key" is a character array of size MAXLINE. It is
 * used to store the key value that is being searched for in the cache.
 *
 * @return a pointer to a block_t structure.
 */
block_t *chceck_cache_repeat(char key[MAXLINE]);

/**
 * The function searches for a cache block with a given key and updates its LRU
 * count if found.
 *
 * @param key The key parameter is a character array (string) with a maximum
 * length of MAXLINE. It is used to search for a specific key in the cache.
 *
 * @return a pointer to a block_t structure.
 */
block_t *search_cache(char key[MAXLINE]);

/**
 * @brief cache_init function initializes the cache by setting the head pointer
 * to NULL, the lru_counter to 0, and the cache_size to 0.
 */
void cache_init();
/**
 * @brief clean cache resource
 * The function `cache_free` frees the memory allocated for a linked list of
 * blocks.
 */
void cache_free();