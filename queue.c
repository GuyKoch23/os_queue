#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include "queue.h"

// items are represented by the ItemNode
// linked list of items waiting in the queue
typedef struct ItemNode {
    void* data;
    int id;
    struct ItemNode* next;
    int isPaired;
} ItemNode;

// Threads are represented by its condition varaible
// linked list of threads waiting to dequeue items from the queue
typedef struct CondVarsNode {
    cnd_t cnd;
    int id;
    struct CondVarsNode* next;
} CondVarsNode;

// The queue structure will hold two linked lists
// ItemNode list for the items in the queue
// CondVarsNode list for the threads waiting to dequeue
typedef struct Queue{
    ItemNode *item_head;
    ItemNode *item_last;
    CondVarsNode *cv_head;
    CondVarsNode *cv_last;
    mtx_t mtx;
    size_t waiting;
    size_t paired; // number of currently paired items in queue to waiting thread which still havent dequeue the item
    size_t visited;
    size_t size;
    size_t count; // kind of id counter for the items inserting to the queue
} Queue;

// The Queue of the app
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
    //free memory for items structs allocated
    while(node != NULL){
        tmp = node;
        node = node->next;
        free(tmp);
    }
    CondVarsNode *node2 = queue.cv_head;
    CondVarsNode *tmp2;
    //free memory for condition vars structs allocated
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
    newNode->isPaired = 0; // indicates if the item already paired to some thread and considered "not available for dequeue"

    // iserting new data to queue
    if(queue.size == 0){ // only item in the queue
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
        while(node != NULL && node->id != -1){ // find first thread can be paired with data
            node = node->next;
        }
        if(node != NULL){
            node->id = newNode->id; // pair the thread with the item
            newNode->isPaired = 1;
            queue.paired++;
            cnd_signal(&node->cnd); // wake up the thread since it can dequeue the data
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
    newNode->id = -1; // sign for un-paried thread

    if(queue.size <= queue.paired){ // no data available - might empty queue or all paired
        cnd_init(&newNode->cnd);
        newNode->next = NULL;

        // insert thread to waiting thread list
        if(queue.cv_head == NULL){ // only thread
            queue.cv_head = newNode;
            queue.cv_last = newNode;
        }
        else{ // insert as last
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
    if(newNode->id != -1){ // the woke up thread is paired, need to find paired item
        while(node != NULL && node->id != newNode->id){
            prev = node;
            node = node->next;
        }
        data = node->data;
        if(prev == NULL){ // remove item from list
            queue.item_head = queue.item_head->next;
            if(queue.item_head == NULL){
                queue.item_last = NULL;
            }
        }
        else{
            prev->next = node->next;
            if(node->next == NULL){
                queue.item_last = prev;
            }
        }
    }
    else{ // thread just arrived, not paired, but item is availble 
        while(node != NULL && node->isPaired == 1){ // skip all paired items
            prev = node;
            node = node->next;
        }
        data = node->data; // first un-paired data
        if(prev == NULL){
            queue.item_head = queue.item_head->next;
            if(queue.item_head == NULL){
                queue.item_last = NULL;
            }
        }
        else{
            prev->next = node->next;
            if(node->next == NULL){
                queue.item_last = prev;
            }
        }
    }
    queue.size--;
    if(newNode->id != -1){
        queue.paired--;
    }
    queue.visited++;
    if(newNode->id != -1){ // remove thread from waiting list
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
    free(newNode);
    free(node);
    mtx_unlock(&queue.mtx);
    return data;
}

bool tryDequeue(void** data){
    mtx_lock(&queue.mtx);
    if(queue.size <= queue.paired){ // no items available
        mtx_unlock(&queue.mtx);
        return false;
    }

    // take appropriate data according to id
    ItemNode *node = queue.item_head;
    ItemNode *prev = NULL;
    while(node->isPaired == 1 || node == NULL){ // find first available item
        prev = node;
        node = node->next;
    }
    if(node == NULL){
        mtx_unlock(&queue.mtx);
        return false;
    }
    *data = node->data;
    if(prev == NULL){ // remove item from list, first item
        queue.item_head = queue.item_head->next;
        if(queue.item_head == NULL){
            queue.item_last = NULL;
        }
    }
    else{// remove item from list, non-first item
        prev->next = node->next;
        if(node->next == NULL){
            queue.item_last = prev;
        }
    }
    queue.size--;
    queue.visited++;
    free(node);
    mtx_unlock(&queue.mtx);
    return true;
}

size_t size(void){
    return queue.size;
}

size_t waiting(void){
    return queue.waiting - queue.paired;
}

size_t visited(void){
    return queue.visited;
}