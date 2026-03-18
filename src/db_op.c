#include "../include/db_op.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int db_init() {
    if (mysql_library_init(0, NULL, NULL)) {
        fprintf(stderr, "could not initialize MySQL library\n");
        return -1;
    }
    return 0;
}

DBConnection* db_connect(Config *conf) {
    DBConnection *db = (DBConnection*)malloc(sizeof(DBConnection));
    if (!db) return NULL;

    db->conn = mysql_init(NULL);
    if (!db->conn) {
        free(db);
        return NULL;
    }

    // 设置字符集为 utf8mb4
    mysql_options(db->conn, MYSQL_SET_CHARSET_NAME, "utf8mb4");

    // 连接数据库
    if (!mysql_real_connect(db->conn, conf->db_host, conf->db_user, 
                            conf->db_pass, conf->db_name, conf->db_port, NULL, 0)) {
        fprintf(stderr, "Connect Error: %s\n", mysql_error(db->conn));
        mysql_close(db->conn);
        free(db);
        return NULL;
    }

    printf("Database connected successfully!\n");
    return db;
}

void db_close(DBConnection *db) {
    if (db) {
        if (db->conn) mysql_close(db->conn);
        free(db);
    }
}

int db_execute_update(DBConnection *db, const char *sql) {
    if (mysql_query(db->conn, sql)) {
        fprintf(stderr, "Update Error: %s\n", mysql_error(db->conn));
        fprintf(stderr, "SQL: %s\n", sql);
        return -1;
    }
    return (int)mysql_affected_rows(db->conn);
}

MYSQL_RES* db_execute_query(DBConnection *db, const char *sql) {
    if (mysql_query(db->conn, sql)) {
        fprintf(stderr, "Query Error: %s\n", mysql_error(db->conn));
        fprintf(stderr, "SQL: %s\n", sql);
        return NULL;
    }
    return mysql_store_result(db->conn);
}

// --- 新增连接池实现 ---

DBPool* db_pool_create(Config *conf, int size) {
    DBPool *pool = (DBPool*)malloc(sizeof(DBPool));
    if (!pool) return NULL;

    pool->connections = (DBConnection*)malloc(sizeof(DBConnection) * size);
    pool->available = (bool*)malloc(sizeof(bool) * size);
    pool->size = size;
    pool->conf = conf; // 保存配置以便重连使用

    if (!pool->connections || !pool->available) {
        free(pool->connections);
        free(pool->available);
        free(pool);
        return NULL;
    }

    // 初始化锁和条件变量
    pthread_mutex_init(&pool->lock, NULL);
    pthread_cond_init(&pool->cond, NULL);

    // 创建连接
    for (int i = 0; i < size; i++) {
        pool->connections[i].index = i;
        pool->available[i] = true; // 初始状态为可用
        
        // 这里复用原有的 db_connect 函数
        MYSQL *conn = mysql_init(NULL);
        if (!conn) {
            fprintf(stderr, "mysql_init failed in pool creation\n");
            continue; // 或者做错误处理
        }
        mysql_options(conn, MYSQL_SET_CHARSET_NAME, "utf8mb4");
        if (!mysql_real_connect(conn, conf->db_host, conf->db_user, 
                                conf->db_pass, conf->db_name, conf->db_port, NULL, 0)) {
            fprintf(stderr, "Connect Error in pool: %s\n", mysql_error(conn));
            pool->connections[i].conn = NULL; // 标记无效
        } else {
            pool->connections[i].conn = conn;
        }
    }

    printf("Database pool created with %d connections.\n", size);
    return pool;
}

// 优化-获取连接：不再持有全局大锁，而是获取独立的连接对象
DBConnection* db_pool_acquire(DBPool *pool) {
    if (!pool || pool->size <= 0) {
        return NULL;
    }
    
    pthread_mutex_lock(&pool->lock);
    
    // 超时计算移至锁内（循环外），确保从“开始等待”时刻计算，避免锁竞争导致的误差
    struct timespec abs_timeout;
    clock_gettime(CLOCK_REALTIME, &abs_timeout);
    abs_timeout.tv_sec += 10;  // 设置绝对超时时间点（当前时间 + 10秒）
    
    while (1) {
        // 遍历寻找可用连接
        for (int i = 0; i < pool->size; i++) {
            if (pool->available[i]) {
                // 如果 conn 为空，说明上次创建或重连失败，跳过
                if (!pool->connections[i].conn) {
                    continue;
                }

                pool->available[i] = false;
                // 找到可用连接，暂时解锁（MySQL操作耗时应尽量在锁外）
                pthread_mutex_unlock(&pool->lock);
                
                // 验证连接是否有效
                if (mysql_ping(pool->connections[i].conn) != 0) {
                    // 连接断开，尝试重连
                    fprintf(stderr, "Connection %d lost, attempting reconnect...\n", i);
                    mysql_close(pool->connections[i].conn);
                    
                    pool->connections[i].conn = mysql_init(NULL);
                    if (pool->connections[i].conn) {
                        mysql_options(pool->connections[i].conn, MYSQL_SET_CHARSET_NAME, "utf8mb4");
                        
                        // 执行数据库连接，使用保存的配置 pool->conf
                        if (!mysql_real_connect(pool->connections[i].conn, 
                                                pool->conf->db_host, 
                                                pool->conf->db_user, 
                                                pool->conf->db_pass, 
                                                pool->conf->db_name, 
                                                pool->conf->db_port, NULL, 0)) {
                            fprintf(stderr, "Reconnect failed: %s\n", mysql_error(pool->connections[i].conn));
                            mysql_close(pool->connections[i].conn);
                            pool->connections[i].conn = NULL; // 标记为无效
                            
                            // 重连失败，需要重新加锁并寻找下一个
                            pthread_mutex_lock(&pool->lock);
                            pool->available[i] = false; // 保持占用状态，防止其他线程拿到无效句柄
                            continue; // 继续循环找下一个
                        }
                    } else {
                        fprintf(stderr, "mysql_init failed during reconnect\n");
                        pthread_mutex_lock(&pool->lock);
                        pool->available[i] = false;
                        continue;
                    }
                }
                
                // 验证或重连成功，返回连接
                return &pool->connections[i];
            }
        }
        
        // 无可用连接，等待（带绝对超时）
        int ret = pthread_cond_timedwait(&pool->cond, &pool->lock, &abs_timeout);
        if (ret == ETIMEDOUT) {
            pthread_mutex_unlock(&pool->lock);
            fprintf(stderr, "DB pool acquire timeout\n");
            return NULL;
        }
    }
}




// 归还连接：标记为可用，唤醒等待线程
void db_pool_release(DBPool *pool, DBConnection *conn) {
    if (!conn) return;

    pthread_mutex_lock(&pool->lock);
    pool->available[conn->index] = true;
    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->lock);
}

void db_pool_destroy(DBPool *pool) {
    if (!pool) return;
    
    for (int i = 0; i < pool->size; i++) {
        if (pool->connections[i].conn) {
            mysql_close(pool->connections[i].conn);
        }
    }
    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->cond);
    
    free(pool->connections);
    free(pool->available);
    free(pool);
}

// 转义函数保持不变，因为传入的 db 现在是线程独占的，所以线程安全
unsigned long db_escape_string(DBConnection *db, char *to, const char *from, unsigned long length) {
    return mysql_real_escape_string(db->conn, to, from, length);
}