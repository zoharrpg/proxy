#include "cache.h"
#include <string.h>
#include <stdlib.h>





static block_t *head = NULL;
static int lru_counter = 0;
static int block_count = 0;


block_t *search_evict_block(){
    block_t* min_lru_block = head;
    block_t *current;

    for (current= head;current!=NULL;current = current->next){
        if(current->lru_count < min_lru_block->lru_count){
            min_lru_block = current;
        }


    }
    return min_lru_block;
}

block_t* block_init(char key[MAXLINE],char value[MAX_OBJECT_SIZE],size_t length){
    block_t* block = malloc(sizeof(block_t));
    memcpy(block->key,key,sizeof(MAXLINE));
    memcpy(block->value,value,sizeof(MAXLINE));
    lru_counter++;
    block->lru_count = lru_counter;
    block->value_length = length;
    block->next = NULL;
    block->prev =NULL;
    return block;

}
void remove_block(block_t* block){
    block_t* prev = block->prev;
    block_t* next = block->next;

    if(prev == NULL && next == NULL){
        head = NULL;
        free(block);
    }
    else if(prev!=NULL && next == NULL){
        free(block);
    }
    else if(prev ==NULL && next == NULL){
        head = block->next;
        free(block);
    }
    else{
        prev->next = next;
        next->prev = prev;
        free(block);
    }
    block_count--;
    return;

}


void add_block(char key[MAXLINE],char value[MAX_OBJECT_SIZE],size_t length){

    if(block_count == MAX_BLOCK_COUNT){
        block_t *evict = search_evict_block();

        if(evict!=NULL){
            remove_block(evict);
        }else{
            sio_printf("evict_error\n");
        }




    }

    block_t* block = block_init(key,value,length);

    if(head == NULL){
        head = block;

    }
    else{
        block->next = head;
        head->prev = block;
        head = block;


    }
    block_count++;
    return;


}



block_t* search_cache(char key[MAXLINE]){
    lru_counter++;

    block_t* current;

    for (current = head; current!= NULL;current=current->next){
        sio_printf("The current is %s\n",current->key);
        if(!strcmp (current->key,key)){
            sio_printf("Searching current\n");
            current->lru_count = lru_counter;
            return current;
        }
    }
    return NULL;

    

}



