#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include "queue.h"

typedef struct ItemNode {
    void* data;
    int id;
    struct ItemNode* next;
    int isPaired;
} ItemNode;

typedef struct CondVarsNode {
    cnd_t cnd;
    int id;
    struct CondVarsNode* next;
} CondVarsNode;

typedef struct Queue{
    ItemNode *item_head;
    ItemNode *item_last;
    CondVarsNode *cv_head;
    CondVarsNode *cv_last;
    mtx_t mtx;
    size_t waiting;
    size_t paired;
    size_t visited;
    size_t size;
    size_t count;
} Queue;


Queue queue;


void initQueue(void){
    queue.waiting = 0;
    queue.paired = 0;
    queue.visited = 0;
    queue.size = 0;
    queue.count = 0;
    queue.item_head = NULL;
    queue.item_last = NULL;
    queue.cv_head = NULL;
    queue.cv_last = NULL;
    mtx_init(&queue.mtx, mtx_plain);
}

void destroyQueue(void){
    ItemNode *node = queue.item_head;
    ItemNode *tmp;
    while(node != NULL){
        tmp = node;
        node = node->next;
        free(tmp);
    }
    CondVarsNode *node2 = queue.cv_head;
    CondVarsNode *tmp2;
    while(node2 != NULL){
        tmp2 = node2;
        node2 = node2->next;
        cnd_destroy(&tmp2->cnd);
        free(tmp2);
    }

    queue.item_head = NULL;
    queue.item_last = NULL;
    queue.cv_last = NULL;
    queue.cv_head = NULL;
    queue.waiting = 0;
    queue.paired = 0;
    queue.visited = 0;
    queue.size = 0;
    queue.count = 0;

    mtx_destroy(&queue.mtx);
}

void enqueue(void* data){
    mtx_lock(&queue.mtx);
    ItemNode *newNode = (ItemNode*)malloc(sizeof(ItemNode));
    if(newNode == NULL){
        fprintf(stderr, "Allocation problem");
        exit(EXIT_FAILURE);
    }

    newNode->data = data;
    newNode->id = queue.count++;
    newNode->next = NULL;
    newNode->isPaired = 0;

    // iserting new data to queue
    if(queue.size == 0){
        queue.item_head = newNode;
        queue.item_last = newNode;
    }
    else{
        queue.item_last->next = newNode; // we want to be able to remove the head so it will point to the next one remains in queue
        queue.item_last = newNode;
    }
    queue.size++;

    // pair data with waiting thread and wake it up
    if(queue.waiting > queue.paired){
        CondVarsNode *node = queue.cv_head;
        while(node != NULL && node->id != -1){
            node = node->next;
        }
        if(node != NULL){
            node->id = newNode->id; // pair the thread with the item
            newNode->isPaired = 1;
            queue.paired++;
            cnd_signal(&node->cnd);
        }
    }
    mtx_unlock(&queue.mtx);
}

void* dequeue(void){
    mtx_lock(&queue.mtx);
    CondVarsNode *newNode = (CondVarsNode*)malloc(sizeof(CondVarsNode));
    if(newNode == NULL){
        fprintf(stderr, "Allocation problem");
        exit(EXIT_FAILURE);
    }
    newNode->id = -1;

    //if(queue.size == 0 || ((queue.waiting >= queue.paired) && queue.waiting > 0)){
    if(queue.size <= queue.paired){
        cnd_init(&newNode->cnd);
        newNode->next = NULL;

        // insert thread to waiting thread list
        if(queue.cv_head == NULL){
            queue.cv_head = newNode;
            queue.cv_last = newNode;
        }
        else{
            queue.cv_last->next = newNode;
            queue.cv_last = newNode;
        }
        queue.waiting++;
        cnd_wait(&newNode->cnd, &queue.mtx);
        cnd_destroy(&newNode->cnd);
        queue.waiting--;
    }

    // take appropriate data according to id
    ItemNode *node = queue.item_head;
    ItemNode *prev = NULL;
    void* data = NULL;
    if(newNode->id != -1){ // sleeping-paired thread which woke up
        while(node != NULL && node->id != newNode->id){
            prev = node;
            node = node->next;
        }
        data = node->data;
        if(prev == NULL){
            queue.item_head = queue.item_head->next;
            if(queue.item_head == NULL){
                queue.item_last = NULL;
            }
        }
        else{
            prev->next = node->next;
            if(node->next == NULL){ // item_last
                queue.item_last = prev;
            }
        }
    }
    else{
        while(node != NULL && node->isPaired == 1){
            prev = node;
            node = node->next;
        }
        data = node->data;
        if(prev == NULL){
            queue.item_head = queue.item_head->next;
            if(queue.item_head == NULL){
                queue.item_last = NULL;
            }
        }
        else{
            prev->next = node->next;
            if(node->next == NULL){ // item_last
                queue.item_last = prev;
            }
        }
    }
    queue.size--;
    if(newNode->id != -1){
        queue.paired--;
    }
    queue.visited++;
    if(newNode->id != -1){
        CondVarsNode *cvnode = queue.cv_head;
        CondVarsNode *cvnode_prev = NULL;
        while(cvnode != NULL && cvnode->id != newNode->id){
            cvnode_prev = cvnode;
            cvnode = cvnode->next;
        }
        if(cvnode_prev == NULL){
            queue.cv_head = queue.cv_head->next;
            if(queue.cv_head == NULL){
                queue.cv_last = NULL;
            }
        }
        else{
            cvnode_prev->next = cvnode->next;
            if(cvnode->next == NULL){
                queue.cv_last = cvnode_prev;
            }
        }
    }
    mtx_unlock(&queue.mtx);
    return data;
}

bool tryDequeue(void** data){
    mtx_lock(&queue.mtx);
    if(queue.size == 0 || ((queue.waiting >= queue.paired) && queue.waiting > 0)){
        mtx_unlock(&queue.mtx);
        return false;
    }

    // take appropriate data according to id
    ItemNode *node = queue.item_head;
    ItemNode *prev = NULL;
    while(node->isPaired == 1 || node == NULL){
        prev = node;
        node = node->next;
    }
    if(node == NULL){
        mtx_unlock(&queue.mtx);
        return false;
    }
    *data = node->data;
    if(prev == NULL){
        queue.item_head = queue.item_head->next;
        if(queue.item_head == NULL){
            queue.item_last = NULL;
        }
    }
    else{
        prev->next = node->next;
    }
    queue.size--;
    queue.visited++;
    //free(node);
    mtx_unlock(&queue.mtx);
    return true;
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