#include "../include/conf.h"
#include "../include/db_op.h"
#include "../include/server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

Config g_conf;

DBPool *g_db_pool; // 替换全局连接为连接池


int main() {
    // 1. 加载配置
    if (load_config("config/cloud_disk.conf", &g_conf) != 0) return -1;

    // 2. 初始化数据库
    if (db_init() != 0) return -1;

    // 获取cpu核心数
    int cpu_cores = sysconf(_SC_NPROCESSORS_ONLN);
    // 线程池大小 = cpu核心数 * 2
    int thread_count = cpu_cores * 2;

    // 连接池大小 = 线程池大小 * 1.5，确保不成为瓶颈
    int pool_size = thread_count + (thread_count / 2);
    // 至少保证 16 个连接
    pool_size = pool_size < 16 ? 16 : pool_size;

    printf("CPU cores: %d, Thread pool size: %d, DB pool size: %d\n", 
        cpu_cores, thread_count, pool_size);

    g_db_pool = db_pool_create(&g_conf, pool_size);
    // g_db = db_connect(&g_conf);
    if (!g_db_pool) {
        fprintf(stderr, "DB pool create failed.\n");
        return -1;
    }


    printf("CPU cores: %d, Thread pool size: %d\n", cpu_cores, thread_count);
    // 3. 启动服务器 (端口 8080, 线程池大小根据CPU核心数自动调整)
    start_server(8080, thread_count);

    // 清理资源 
    db_pool_destroy(g_db_pool);
    mysql_library_end();

    return 0;
}