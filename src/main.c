#include "../include/conf.h"
#include "../include/db_op.h"
#include "../include/server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

Config g_conf;
DBConnection *g_db;
pthread_mutex_t g_db_lock = PTHREAD_MUTEX_INITIALIZER;

int main() {
    // 1. 加载配置
    if (load_config("config/cloud_disk.conf", &g_conf) != 0) return -1;

    // 2. 初始化数据库
    if (db_init() != 0) return -1;
    g_db = db_connect(&g_conf);
    if (!g_db) {
        fprintf(stderr, "DB connect failed.\n");
        return -1;
    }

    // 获取cpu核心数
    int cpu_cores = sysconf(_SC_NPROCESSORS_ONLN);
    // 线程池大小 = cpu核心数 * 2
    int thread_count = cpu_cores * 2;
    printf("CPU cores: %d, Thread pool size: %d\n", cpu_cores, thread_count);

    // 3. 启动服务器 (端口 8080, 线程池大小根据CPU核心数自动调整)
    start_server(8080, thread_count);

    // 清理资源 (实际上 start_server 是死循环，不会执行到这里)
    db_close(g_db);
    mysql_library_end();

    return 0;
}