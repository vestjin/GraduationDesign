#ifndef _DB_OP_H
#define _DB_OP_H

#include "conf.h"
#include <mysql/mysql.h>
#include <pthread.h>
#include <stdbool.h>


// 数据库连接句柄包装（为未来连接池预留结构）
typedef struct {
    MYSQL *conn;
    int index; // 连接池索引，用于调试
} DBConnection;

// 数据库连接池结构体
typedef struct {
    DBConnection *connections;  // 连接数组
    bool*available;  // 连接是否可用标记
    int size;        // 连接池大小
    pthread_mutex_t lock; // 连接池锁-仅用于管理池资源，不用于业务串行化
    pthread_cond_t cond;   // 条件变量-用于等待可用连接
} DBPool;

// 初始化数据库环境（如 mysql_library_init）
int db_init();

// 连接数据库
DBConnection* db_connect(Config *conf);

// 关闭连接
void db_close(DBConnection *db);

// 执行非查询语句 (INSERT, UPDATE, DELETE)
// 返回: -1失败, >=0 影响的行数
int db_execute_update(DBConnection *db, const char *sql);

// 执行查询语句 (SELECT)
// 返回: MYSQL_RESULT 指针，使用后需调用 mysql_free_result
MYSQL_RES* db_execute_query(DBConnection *db, const char *sql);

// 转义字符串，防止 SQL 注入
// to: 输出缓冲区
// from: 输入字符串
// 返回: 转义后的长度
unsigned long db_escape_string(DBConnection *db, char *to, const char *from, unsigned long length);

// --- 新增连接池管理接口 ---

// 创建连接池
// size: 池中连接数量
DBPool* db_pool_create(Config *conf, int size);

// 从池中获取一个可用连接（阻塞直到有空闲连接）
DBConnection* db_pool_acquire(DBPool *pool);

// 归还连接到池中
void db_pool_release(DBPool *pool, DBConnection *conn);

// 销毁连接池
void db_pool_destroy(DBPool *pool);

#endif