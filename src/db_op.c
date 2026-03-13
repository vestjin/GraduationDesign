#include "../include/db_op.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// 【关键优化】获取连接：不再持有全局大锁，而是获取独立的连接对象
DBConnection* db_pool_acquire(DBPool *pool) {
    pthread_mutex_lock(&pool->lock);

    while (1) {
        for (int i = 0; i < pool->size; i++) {
            if (pool->available[i] && pool->connections[i].conn) {
                pool->available[i] = false;
                pthread_mutex_unlock(&pool->lock);
                return &pool->connections[i];
            }
        }
        // 暂无可用连接，等待条件变量
        printf("[Pool] Waiting for available connection...\n");
        pthread_cond_wait(&pool->cond, &pool->lock);
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