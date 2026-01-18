#ifndef _FILE_HANDLER_H
#define _FILE_HANDLER_H

#include "../include/db_op.h"
#include "../libs/cJSON/cJSON.h"
#include "../include/protocol.h"


// 验证 Token 是否有效
// 返回: 用户ID (>0) 成功, -1 失败
long verify_user_token(DBConnection *db, const char *token, pthread_mutex_t *db_lock);
// 辅助：检查剩余配额是否足够
// 返回 1-足够, 0-不足, -1-错误
// static int check_quota_enough(DBConnection *db, long user_id, long long file_size);
// 预检查上传（秒传/断点续传）
// req_json 包含: md5, file_size, file_name, parent_id
cJSON* handle_upload_check(DBConnection *db, long user_id, const cJSON *req_json, pthread_mutex_t *db_lock);

// 完成上传（更新元数据和配额）
// req_json 包含: md5, file_path (服务器上的实际存储路径)
cJSON* handle_upload_complete(DBConnection *db, long user_id, const cJSON *req_json, pthread_mutex_t *db_lock);

// 处理文件/目录列表请求
// req_json 应包含 parent_id (整数, 0代表根目录)
cJSON* handle_file_list(DBConnection *db, long user_id, const cJSON *req_json, pthread_mutex_t *db_lock);

// 处理创建文件夹请求
// req_json 应包含 parent_id 和 folder_name
cJSON* handle_file_mkdir(DBConnection *db, long user_id, const cJSON *req_json, pthread_mutex_t *db_lock);

// 下载
void handle_file_download(int client_fd, DBConnection *db, long user_id, const cJSON *req_json, pthread_mutex_t *db_lock);
// 分片上传
void handle_upload_chunk(int client_fd, DBConnection *db, long user_id, HttpRequest *req, pthread_mutex_t *db_lock);

cJSON* handle_file_rename(DBConnection *db, long user_id, const cJSON *req_json, pthread_mutex_t *db_lock);
cJSON* handle_file_delete(DBConnection *db, long user_id, const cJSON *req_json, pthread_mutex_t *db_lock);
cJSON* handle_file_move(DBConnection *db, long user_id, const cJSON *req_json, pthread_mutex_t *db_lock);


#endif