#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include "queue.h"

typedef struct ItemNode {
    void* data;
    struct ItemNode* next;
} ItemNode;

typedef struct CondVarsNode {
    cnd_t cnd;
    struct CondVarsNode* next;
} CondVarsNode;

typedef struct Queue{
    ItemNode *item_head;
    ItemNode *item_last;
    CondVarsNode *cv_head;
    CondVarsNode *cv_last;
    mtx_t mtx;
    size_t waiting;
    size_t visited;
    size_t size;
} Queue;


Queue queue;


void initQueue(void){
    queue.waiting = 0;
    queue.visited = 0;
    queue.size = 0;
    queue.item_head = NULL;
    queue.item_last = NULL;
    queue.cv_head = NULL;
    queue.cv_last = NULL;
    mtx_init(&queue.mtx, mtx_plain);
}

void destroyQueue(void){
    mtx_destroy(&queue.mtx);
    mtx_destroy(&queue.mtx);
    // free memory of itemsQ
    ItemNode *node = queue.item_head;
    ItemNode *tmp;
    while(node != NULL){
        tmp = node;
        node = node->next;
        free(tmp);
    }
    //free memory of condvarsQ
    CondVarsNode *node2 = queue.cv_head;
    CondVarsNode *tmp2;
    while(node2 != NULL){
        tmp2 = node2;
        node2 = node2->next;
        free(tmp2);
    }

    queue.item_head = NULL;
    queue.item_last = NULL;
    queue.cv_last = NULL;
    queue.cv_head = NULL;
    queue.waiting = 0;
    queue.visited = 0;
    queue.size = 0;
}

void enqueue(void* data){
    mtx_lock(&queue.mtx);
    ItemNode *newNode = (ItemNode*)malloc(sizeof(ItemNode));
    if(newNode == NULL){
        fprintf(stderr, "Allocation problem");
        exit(EXIT_FAILURE);
    }
    newNode->data = data;
    newNode->next = NULL;

    if(queue.item_head == NULL){ // queue is empty
        queue.item_head = newNode;
        queue.item_last = newNode;
    }
    else{ // queue is not empty
        queue.item_last->next = newNode; // we want to be able to remove the head so it will point to the next one remains in queue
        queue.item_last = newNode;
    }
    queue.size++;
    
    // wake up first waiting thread if some wait
    if(queue.cv_head != NULL){
        CondVarsNode *node = queue.cv_head;
        queue.cv_head = queue.cv_head->next;
        cnd_signal(&node->cnd);
        free(node);
    }
    mtx_unlock(&queue.mtx);
}


size_t size(void){
    return queue.size;
}

size_t waiting(void){
    return queue.waiting;
}

size_t visited(void){
return queue.visited;
}