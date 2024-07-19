#include <stdio.h>
#include <threads.h>
#include "queue.h"

typedef struct ItemNode {
    void* data;
    struct Node* next;
} ItemNode;

typedef struct CondVarsNode {
    cnd_t cnd;
    struct Node* next;
} CondVarsNode;

typedef struct ItemsQueue{
    ItemNode *head;
    ItemNode *last;
    mtx_t mtx;
    size_t waiting;
    size_t visited;
    size_t size;
} ItemsQueue;

typedef struct CondVarsQueue{
    CondVarsNode *head;
    CondVarsNode *last;
    mtx_t mtx;
    size_t waiting;
    size_t visited;
} CondVarsQueue;

ItemsQueue itemsQ;

CondVarsQueue condVarsQ;

void initQueue(void){
    itemsQ.waiting = 0;
    itemsQ.visited = 0;
    itemsQ.size = 0;
    condVarsQ.waiting = 0;
    condVarsQ.visited = 0;
}

void destroyQueue(void){
    itemsQ.head = NULL;
    itemsQ.last = NULL;
    condVarsQ.head = NULL;
    condVarsQ.last = NULL;
}


size_t size(void){
    return itemsQ.size;
}

size_t waiting(void){
    return itemsQ.waiting;
}

size_t visited(void){
return itemsQ.visited;
}