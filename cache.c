#include "cache.h"
#include <stdlib.h>
#include <string.h>

static block_t *head = NULL;
static int lru_counter = 0;
static size_t cache_size = 0;
static size_t blcok_count = 0;

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
    blcok_count--;
    return;
}

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
    blcok_count++;
    sio_printf("the block count is %ld\n", blcok_count);
    return;
}

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

block_t *search_cache(char key[MAXLINE]) {

    block_t *current;

    for (current = head; current != NULL; current = current->next) {
        // sio_printf("The current is %s\n",current->key);
        if (!strcmp(current->key, key)) {
            lru_counter++;
            sio_printf("Searching current\n");
            current->lru_count = lru_counter;
            return current;
        }
    }
    return NULL;
}

void cache_init() {
    head = NULL;
    lru_counter = 0;
    cache_size = 0;
    blcok_count = 0;
}

void cache_free() {
    block_t *current = NULL;
    while (current != NULL) {
        block_t *next = current->next;
        free(current);
        current = next;
    }
}
