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

unsigned long db_escape_string(DBConnection *db, char *to, const char *from, unsigned long length) {
    return mysql_real_escape_string(db->conn, to, from, length);
}