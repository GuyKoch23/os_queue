#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <stdatomic.h>
#include <assert.h>
#include "queue.h"

// Helper function to print test results
void print_result(const char *test_name, bool result) {
    printf("%s: %s\n", test_name, result ? "PASSED" : "FAILED");
}

// Function to test basic functionality of the queue
void test_basic_functionality() {
    initQueue();

    // Test enqueue and size
    enqueue((void *)(long)1);
    enqueue((void *)(long)2);
    enqueue((void *)(long)3);
    print_result("Basic Functionality - Size after 3 enqueues", size() == 3);

    // Test dequeue
    assert((long)dequeue() == 1);
    assert((long)dequeue() == 2);
    assert((long)dequeue() == 3);
    print_result("Basic Functionality - Dequeue", size() == 0);

    // Test visited
    print_result("Basic Functionality - Visited after 3 enqueues and 3 dequeues", visited() == 3);

    destroyQueue();
}

// Function to test edge cases of the queue
void test_edge_cases() {
    initQueue();

    // Test dequeue on empty queue (should block)
    thrd_t thread;
    bool thread_finished = false;

    // Thread function to dequeue from an empty queue
    int dequeue_thread(void *arg) {
        (void)arg;
        dequeue();
        thread_finished = true;
        return 0;
    }

    thrd_create(&thread, dequeue_thread, NULL);
    thrd_sleep(&(struct timespec){.tv_sec = 1, .tv_nsec = 0}, NULL);
    print_result("Edge Case - Dequeue on empty queue blocks", !thread_finished);

    // Enqueue to unblock the thread
    enqueue((void *)(long)1);

    thrd_join(thread, NULL);

    print_result("Edge Case - Dequeue on empty queue unblocks", thread_finished);

    // Test tryDequeue on empty queue
    void *item;
    void *former_item = item;
    bool result = tryDequeue(&item);
    print_result("Edge Case - TryDequeue on empty queue", !result && item == former_item);

    // Test tryDequeue on non-empty queue
    enqueue((void *)(long)2);
    result = tryDequeue(&item);
    print_result("Edge Case - TryDequeue on non-empty queue", result && (long)item == 2);

    destroyQueue();
}

// Function to test concurrency
void test_concurrency() {
    initQueue();

    const int num_threads = 5;
    const int num_items_per_thread = 1000;
    thrd_t threads[num_threads];
    atomic_size_t counter = ATOMIC_VAR_INIT(0);

    // Thread function to enqueue items
    int enqueue_thread(void *arg) {
        long id = (long)arg;
        for (int i = 0; i < num_items_per_thread; ++i) {
            enqueue((void *)(id * num_items_per_thread + i));
        }
        return 0;
    }

    // Thread function to dequeue items
    int dequeue_thread(void *arg) {
        (void)arg;
        for (int i = 0; i < num_items_per_thread; ++i) {
            dequeue();
            atomic_fetch_add(&counter, 1);
        }
        return 0;
    }

    // Create enqueue threads
    for (long i = 0; i < num_threads; ++i) {
        thrd_create(&threads[i], enqueue_thread, (void *)i);
    }

    // Wait for enqueue threads to finish
    for (int i = 0; i < num_threads; ++i) {
        thrd_join(threads[i], NULL);
    }

    // Check if the size of the queue is correct
    print_result("Concurrency - Size after enqueues", size() == num_threads * num_items_per_thread);

    // Create dequeue threads
    for (int i = 0; i < num_threads; ++i) {
        thrd_create(&threads[i], dequeue_thread, NULL);
    }

    // Wait for dequeue threads to finish
    for (int i = 0; i < num_threads; ++i) {
        thrd_join(threads[i], NULL);
    }

    // Check if all items were dequeued
    print_result("Concurrency - Dequeue all items", counter == num_threads * num_items_per_thread);

    destroyQueue();
}

// Function to test visited count
void test_visited_count() {
    initQueue();

    // Enqueue and dequeue multiple items
    const int num_items = 10;
    for (int i = 0; i < num_items; ++i) {
        enqueue((void *)(long)i);
    }
    for (int i = 0; i < num_items; ++i) {
        dequeue();
    }

    print_result("Visited Count - After 10 enqueues and dequeues", visited() == num_items);

    // Enqueue and dequeue again to check if the visited count accumulates correctly
    for (int i = 0; i < num_items; ++i) {
        enqueue((void *)(long)i);
    }
    printf("aaa % ld aaa\n", size());
    for (int i = 0; i < num_items; ++i) {
        dequeue();
    }

    print_result("Visited Count - After 20 enqueues and dequeues", visited() == 2 * num_items);

    destroyQueue();
}

// Function to stress test the queue with a large number of operations
void stress_test() {
    initQueue();

    const int num_threads = 4;  // Reduce number of threads
    const int num_operations = 1000;  // Reduce number of operations
    thrd_t threads[num_threads];
    atomic_size_t enqueue_counter = ATOMIC_VAR_INIT(0);
    atomic_size_t dequeue_counter = ATOMIC_VAR_INIT(0);

    // Thread function to perform enqueue operations
    int enqueue_thread(void *arg) {
        (void)arg;
        for (int i = 0; i < num_operations; ++i) {
            enqueue((void *)(long)i);
            atomic_fetch_add(&enqueue_counter, 1);
        }
        return 0;
    }

    // Thread function to perform dequeue operations
    int dequeue_thread(void *arg) {
        (void)arg;
        for (int i = 0; i < num_operations; ++i) {
            dequeue();
            atomic_fetch_add(&dequeue_counter, 1);
        }
        return 0;
    }

    // Create enqueue threads
    for (int i = 0; i < num_threads / 2; ++i) {
        thrd_create(&threads[i], enqueue_thread, NULL);
    }

    // Create dequeue threads
    for (int i = num_threads / 2; i < num_threads; ++i) {
        thrd_create(&threads[i], dequeue_thread, NULL);
    }

    // Wait for all threads to finish
    for (int i = 0; i < num_threads; ++i) {
        thrd_join(threads[i], NULL);
    }
    printf("eeeeee\n");

    print_result("Stress Test - Total enqueues", enqueue_counter == (num_threads / 2) * num_operations);
    print_result("Stress Test - Total dequeues", dequeue_counter == (num_threads / 2) * num_operations);

    destroyQueue();
}

// Function to test FIFO order
void test_fifo_order() {
    initQueue();

    // Enqueue multiple items
    for (int i = 1; i <= 5; ++i) {
        enqueue((void *)(long)i);
    }

    // Dequeue and check order
    bool fifo_order = true;
    for (int i = 1; i <= 5; ++i) {
        if ((long)dequeue() != i) {
            fifo_order = false;
            break;
        }
    }

    print_result("FIFO Order Test", fifo_order);

    destroyQueue();
}

// Function to test multiple threads enqueuing and dequeuing
void test_multiple_threads() {
    initQueue();

    const int num_threads = 10;
    const int num_items_per_thread = 100;
    thrd_t threads[num_threads];
    atomic_size_t counter = ATOMIC_VAR_INIT(0);

    // Thread function to enqueue and dequeue items
    int thread_func(void *arg) {
        long id = (long)arg;
        for (int i = 0; i < num_items_per_thread; ++i) {
            enqueue((void *)(id * num_items_per_thread + i));
        }
        for (int i = 0; i < num_items_per_thread; ++i) {
            dequeue();
            atomic_fetch_add(&counter, 1);
        }
        return 0;
    }

    // Create threads
    for (long i = 0; i < num_threads; ++i) {
        thrd_create(&threads[i], thread_func, (void *)i);
    }

    // Wait for threads to finish
    for (int i = 0; i < num_threads; ++i) {
        thrd_join(threads[i], NULL);
    }

    // Check if all items were dequeued
    print_result("Multiple Threads Test - Dequeue all items", counter == num_threads * num_items_per_thread);

    destroyQueue();
}

// Function to test large data
void test_large_data() {
    initQueue();

    const int num_items = 100000;
    for (int i = 0; i < num_items; ++i) {
        enqueue((void *)(long)i);
    }

    bool large_data_correct = true;
    for (int i = 0; i < num_items; ++i) {
        if ((long)dequeue() != i) {
            large_data_correct = false;
            break;
        }
    }

    print_result("Large Data Test", large_data_correct);

    destroyQueue();
}

// Function to test random operations
void test_random_operations() {
    initQueue();

    const int num_operations = 1000;
    thrd_t threads[2];

    // Thread function to perform random enqueue/dequeue operations
    int random_operations(void *arg) {
        (void)arg;
        for (int i = 0; i < num_operations; ++i) {
            if (rand() % 2) {
                enqueue((void *)(long)i);
            } else if (size() > 0) {
                dequeue();
            }
        }
        return 0;
    }

    // Create threads
    for (int i = 0; i < 2; ++i) {
        thrd_create(&threads[i], random_operations, NULL);
        thrd_sleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 5000}, NULL);
    }

    // Wait for threads to finish
    for (int i = 0; i < 2; ++i) {
        thrd_join(threads[i], NULL);
    }

    print_result("Random Operations Test - No deadlocks", true);

    destroyQueue();
}

// Function to test that threads wake up in correct order
void test_thread_wakeup_order() {
    initQueue();

    const int num_threads = 3;
    thrd_t threads[num_threads];
    atomic_size_t wakeup_order[num_threads];
    atomic_size_t wakeup_index = ATOMIC_VAR_INIT(0);

    // Thread function to dequeue and record wakeup order
     int dequeue_and_record(void *arg) {
        long id = (long)arg;
        dequeue();
        wakeup_order[atomic_fetch_add(&wakeup_index, 1)] = id;
        printf("%d", id);
        return 0;
    }

    // Create threads
    for (long i = 0; i < num_threads; ++i) {
        thrd_create(&threads[i], dequeue_and_record, (void *)i);
        thrd_sleep(&(struct timespec){.tv_sec = 1, .tv_nsec = 0}, NULL);
    }

    // Enqueue items to wake up the threads
    for (int i = 0; i < num_threads; ++i) {
        enqueue((void *)(long)i);
    }
    // Wait for threads to finish
    for (int i = 0; i < num_threads; ++i) {
        thrd_join(threads[i], NULL);
    }

    // Check if threads were woken up in the correct order
    bool correct_order = true;
    for (int i = 0; i < num_threads; ++i) {
        if (wakeup_order[i] != i) {
            // printf("%d %d\n", wakeup_order[i], i);
            correct_order = false;
            break;
        }
    }

    print_result("Thread Wakeup Order Test", correct_order);

    destroyQueue();
}

int main() {
    test_basic_functionality();
    test_edge_cases();
    test_concurrency();
    test_visited_count();
    stress_test();
    test_fifo_order();
    test_multiple_threads();
    test_large_data();
    test_random_operations();
    test_thread_wakeup_order();

    return 0;
}