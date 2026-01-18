#ifndef _USER_HANDLER_H
#define _USER_HANDLER_H

#include "../include/db_op.h"
#include "../libs/cJSON/cJSON.h"

// 业务状态码定义
#define USER_OK 0
#define USER_EXIST 1001
#define USER_NOT_FOUND 1002
#define USER_PASS_WRONG 1003
#define USER_DB_ERROR 1004

// 处理注册请求
// 参数: db连接, json请求体
// 返回: cJSON 对象指针(包含响应数据)，需调用者释放
cJSON* handle_register(DBConnection *db, const cJSON *req_json);

// 处理登录请求
// 参数: db连接, json请求体, client_ip (用于日志)
// 返回: cJSON 对象指针
cJSON* handle_login(DBConnection *db, const cJSON *req_json, const char *client_ip);

#endif