#ifndef _DB_OP_H
#define _DB_OP_H

#include "conf.h"
#include <mysql/mysql.h>

// 数据库连接句柄包装（为未来连接池预留结构）
typedef struct {
    MYSQL *conn;
} DBConnection;

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



#endif