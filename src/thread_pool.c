#include "../include/thread_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

// 任务结构体
typedef struct task_t {
    thread_func_t function;
    void *arg;
    struct task_t *next;
} task_t;

// 线程池结构体
struct threadpool_t {
    pthread_mutex_t lock;
    pthread_cond_t notify;
    pthread_t *threads;
    task_t *queue_head;
    task_t *queue_tail;
    int thread_count;
    int queue_size;
    int shutdown;
};

// 工作线程的回调函数
static void *threadpool_worker(void *arg) {
    threadpool_t *pool = (threadpool_t *)arg;
    task_t *task;

    while (1) {
        // 加锁
        pthread_mutex_lock(&(pool->lock));

        // 等待条件变量，直到有任务或 shutdown
        while ((pool->queue_size == 0) && (!pool->shutdown)) {
            pthread_cond_wait(&(pool->notify), &(pool->lock));
        }

        if (pool->shutdown) {
            pthread_mutex_unlock(&(pool->lock));
            pthread_exit(NULL);
        }

        // 取出任务
        task = pool->queue_head;
        if (task == NULL) {
            pthread_mutex_unlock(&(pool->lock));
            continue;
        }
        
        pool->queue_head = task->next;
        pool->queue_size--;
        pthread_mutex_unlock(&(pool->lock));

        // 执行任务
        (task->function)(task->arg);
        free(task);
    }
    return NULL;
}

threadpool_t *threadpool_create(int thread_count) {
    threadpool_t *pool = (threadpool_t *)malloc(sizeof(threadpool_t));
    if (!pool) return NULL;

    pool->thread_count = 0;
    pool->queue_size = 0;
    pool->queue_head = NULL;
    pool->queue_tail = NULL;
    pool->shutdown = 0;

    // 初始化锁和条件变量
    if (pthread_mutex_init(&(pool->lock), NULL) != 0 ||
        pthread_cond_init(&(pool->notify), NULL) != 0) {
        free(pool);
        return NULL;
    }

    // 创建线程
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
    if (!pool->threads) {
        pthread_mutex_destroy(&(pool->lock));
        pthread_cond_destroy(&(pool->notify));
        free(pool);
        return NULL;
    }

    for (int i = 0; i < thread_count; i++) {
        if (pthread_create(&(pool->threads[i]), NULL, threadpool_worker, (void*)pool) != 0) {
            // 创建失败回滚
            threadpool_destroy(pool);
            return NULL;
        }
        pool->thread_count++;
    }

    return pool;
}

int threadpool_add(threadpool_t *pool, thread_func_t function, void *arg) {
    task_t *new_task = (task_t *)malloc(sizeof(task_t));
    if (!new_task) return -1;

    new_task->function = function;
    new_task->arg = arg;
    new_task->next = NULL;

    pthread_mutex_lock(&(pool->lock));

    if (pool->shutdown) {
        pthread_mutex_unlock(&(pool->lock));
        free(new_task);
        return -1;
    }

    // 加入队列尾部
    if (pool->queue_size == 0) {
        pool->queue_head = new_task;
        pool->queue_tail = new_task;
    } else {
        pool->queue_tail->next = new_task;
        pool->queue_tail = new_task;
    }
    pool->queue_size++;

    // 唤醒一个工作线程
    pthread_cond_signal(&(pool->notify));
    pthread_mutex_unlock(&(pool->lock));

    return 0;
}

int threadpool_destroy(threadpool_t *pool) {
    if (pool == NULL) return -1;

    pthread_mutex_lock(&(pool->lock));
    pool->shutdown = 1;
    pthread_mutex_unlock(&(pool->lock));

    // 唤醒所有线程以退出
    pthread_cond_broadcast(&(pool->notify));

    for (int i = 0; i < pool->thread_count; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    free(pool->threads);
    pthread_mutex_destroy(&(pool->lock));
    pthread_cond_destroy(&(pool->notify));
    free(pool);

    return 0;
}