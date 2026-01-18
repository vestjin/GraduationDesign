#ifndef _THREAD_POOL_H
#define _THREAD_POOL_H

typedef struct threadpool_t threadpool_t;

// 任务函数指针类型
typedef void (*thread_func_t)(void *arg);

// 创建线程池
threadpool_t *threadpool_create(int thread_count);

// 向线程池添加任务
int threadpool_add(threadpool_t *pool, thread_func_t function, void *arg);

// 销毁线程池
int threadpool_destroy(threadpool_t *pool);

#endif