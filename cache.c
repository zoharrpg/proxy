/**
 * @file cache.c
 * @brief Doubly linked list cache using LRU evict policy
 * @author Junshang Jia <junshanj@andrew.cmu.edu>
 *
 */

#include "cache.h"
#include <stdlib.h>
#include <string.h>

static block_t *head = NULL;
static int lru_counter = 0;
static size_t cache_size = 0;

/**
 * The function searches for the evict block with the minimum LRU count in a
 * linked list and returns it.
 *
 * @return a pointer to a block_t structure.
 */
block_t *search_evict_block() {
    block_t *min_lru_block = head;
    block_t *current;

    for (current = head; current != NULL; current = current->next) {
        if (current->lru_count < min_lru_block->lru_count) {
            min_lru_block = current;
        }
    }
    return min_lru_block;
}

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
                    size_t length) {
    block_t *block = malloc(sizeof(block_t));
    memcpy(block->key, key, MAXLINE);
    memcpy(block->value, value, MAX_OBJECT_SIZE);
    lru_counter++;
    block->lru_count = lru_counter;
    block->value_length = length;
    block->next = NULL;
    block->prev = NULL;
    return block;
}
/**
 * The function removes a block from a linked list and updates the cache size.
 *
 * @param block The `block` parameter is a pointer to a `block_t` structure.
 *
 */
void remove_block(block_t *block) {
    size_t length = block->value_length;

    block_t *prev = block->prev;
    block_t *next = block->next;

    if (prev == NULL && next == NULL) {
        head = NULL;
        free(block);
    } else if (prev != NULL && next == NULL) {
        prev->next = NULL;
        free(block);
    } else if (prev == NULL && next != NULL) {
        head = next;
        next->prev = NULL;

        free(block);
    } else {
        prev->next = next;
        next->prev = prev;
        block->prev = NULL;
        block->next = NULL;
        free(block);
    }
    cache_size -= length;

    return;
}

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
void add_block(char key[MAXLINE], char value[MAX_OBJECT_SIZE], size_t length) {

    block_t *check = chceck_cache_repeat(key);
    if (check != NULL) {
        return;
    }

    while (cache_size + length > MAX_CACHE_SIZE) {
        block_t *evict = search_evict_block();
        sio_printf("the lru counter %d\n", evict->lru_count);

        if (evict != NULL) {
            remove_block(evict);
        } else {
            sio_printf("evict_error\n");
        }
    }

    block_t *block = block_init(key, value, length);

    if (head == NULL) {
        head = block;

    } else {
        block->next = head;
        head->prev = block;
        head = block;
    }
    cache_size += length;
    return;
}

/**
 * The function checks if a given key exists in a cache and returns the
 * corresponding block if found, otherwise returns NULL.
 *
 * @param key The parameter "key" is a character array of size MAXLINE. It is
 * used to store the key value that is being searched for in the cache.
 *
 * @return a pointer to a block_t structure.
 */
block_t *chceck_cache_repeat(char key[MAXLINE]) {
    block_t *current;

    for (current = head; current != NULL; current = current->next) {
        // sio_printf("The current is %s\n",current->key);
        if (!strcmp(current->key, key)) {

            return current;
        }
    }
    return NULL;
}

/**
 * The function searches for a cache block with a given key and updates its LRU
 * count if found.
 *
 * @param key The key parameter is a character array (string) with a maximum
 * length of MAXLINE. It is used to search for a specific key in the cache.
 *
 * @return a pointer to a block_t structure.
 */
block_t *search_cache(char key[MAXLINE]) {

    block_t *current;

    for (current = head; current != NULL; current = current->next) {
        if (!strcmp(current->key, key)) {
            lru_counter++;
            current->lru_count = lru_counter;
            return current;
        }
    }
    return NULL;
}

/**
 * @brief cache_init function initializes the cache by setting the head pointer
 * to NULL, the lru_counter to 0, and the cache_size to 0.
 */
void cache_init(void) {
    head = NULL;
    lru_counter = 0;
    cache_size = 0;
}

/**
 * @brief clean cache resource
 * The function `cache_free` frees the memory allocated for a linked list of
 * blocks.
 */
void cache_free(void) {
    block_t *current = NULL;
    while (current != NULL) {
        block_t *next = current->next;
        free(current);
        current = next;
    }
}
