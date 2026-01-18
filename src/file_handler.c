#include "../include/file_handler.h"
#include "../include/db_op.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>

// 辅助函数：验证 Token
long verify_user_token(DBConnection *db, const char *token, pthread_mutex_t *db_lock) {
    if (!token || strlen(token) == 0) return -1;

    char sql[2048]; // 【修复】增大缓冲区
    char esc_token[128];
    db_escape_string(db, esc_token, token, strlen(token));

    snprintf(sql, sizeof(sql), 
             "SELECT user_id FROM users WHERE token='%s' AND token_expire > NOW()", esc_token);

    MYSQL_RES *res = NULL;
    pthread_mutex_lock(db_lock);
    res = db_execute_query(db, sql);
    pthread_mutex_unlock(db_lock);

    if (!res) return -1;

    long uid = -1;
    if (mysql_num_rows(res) > 0) {
        MYSQL_ROW row = mysql_fetch_row(res);
        uid = atol(row[0]);
    }
    
    mysql_free_result(res);
    return uid;
}

// 辅助：从 URL 中解析参数
static void get_url_param(const char *url, const char *key, char *output) {
    const char *start = strchr(url, '?');
    if (!start) { output[0] = 0; return; }
    start++; 
    
    char query[512]; 
    strncpy(query, start, sizeof(query));
    query[sizeof(query)-1] = 0;
    
    char *token = strtok(query, "&");
    while (token) {
        size_t key_len = strlen(key);
        if (strncmp(token, key, key_len) == 0 && token[key_len] == '=') {
            strcpy(output, token + key_len + 1);
            return;
        }
        token = strtok(NULL, "&");
    }
    output[0] = 0;
}

cJSON* handle_file_list(DBConnection *db, long user_id, const cJSON *req_json, pthread_mutex_t *db_lock) {
    cJSON *root = cJSON_CreateObject();

    cJSON *pid_obj = cJSON_GetObjectItem(req_json, "parent_id");
    int parent_id = pid_obj ? pid_obj->valueint : 0;

    cJSON *fetch_all_obj = cJSON_GetObjectItem(req_json, "fetch_all");
    int fetch_all = (fetch_all_obj && fetch_all_obj->valueint);

    char sql[2048];
    MYSQL_RES *res = NULL;

    if (fetch_all) {
        snprintf(sql, sizeof(sql), 
                 "SELECT file_id, file_name, file_size, file_type, parent_id "
                 "FROM files WHERE user_id=%ld ORDER BY parent_id ASC, file_type ASC", 
                 user_id);
    } else {
        snprintf(sql, sizeof(sql), 
                 "SELECT file_id, file_name, file_size, file_type, create_time "
                 "FROM files WHERE user_id=%ld AND parent_id=%d ORDER BY file_type ASC, create_time DESC", 
                 user_id, parent_id);
    }

    pthread_mutex_lock(db_lock);
    res = db_execute_query(db, sql);
    pthread_mutex_unlock(db_lock);
    
    if (!res) {
        cJSON_AddNumberToObject(root, "code", 500);
        cJSON_AddStringToObject(root, "msg", "Database error");
        return root;
    }

    cJSON *data_array = cJSON_CreateArray();
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "file_id", atol(row[0]));
        cJSON_AddStringToObject(item, "file_name", row[1]);
        cJSON_AddNumberToObject(item, "file_size", atoll(row[2]));
        cJSON_AddNumberToObject(item, "file_type", atoi(row[3]));
        
        if (!fetch_all) {
            cJSON_AddStringToObject(item, "create_time", row[4]);
        } else {
            cJSON_AddNumberToObject(item, "parent_id", atoi(row[4]));
        }
        cJSON_AddItemToArray(data_array, item);
    }
    mysql_free_result(res);

    cJSON_AddNumberToObject(root, "code", 0);
    cJSON_AddStringToObject(root, "msg", "Success");
    cJSON_AddItemToObject(root, "data", data_array);

    return root;
}

cJSON* handle_file_mkdir(DBConnection *db, long user_id, const cJSON *req_json, pthread_mutex_t *db_lock) {
    cJSON *root = cJSON_CreateObject();
    cJSON *name_obj = cJSON_GetObjectItem(req_json, "folder_name");
    cJSON *pid_obj = cJSON_GetObjectItem(req_json, "parent_id");

    if (!name_obj) {
        cJSON_AddNumberToObject(root, "code", 400);
        cJSON_AddStringToObject(root, "msg", "Missing folder_name");
        return root;
    }
    const char *folder_name = name_obj->valuestring;
    int parent_id = pid_obj ? pid_obj->valueint : 0;

    char sql[2048];
    char esc_name[256];
    db_escape_string(db, esc_name, folder_name, strlen(folder_name));
    
    snprintf(sql, sizeof(sql), 
             "SELECT file_id FROM files WHERE user_id=%ld AND parent_id=%d AND file_name='%s' AND file_type=1", 
             user_id, parent_id, esc_name);

    MYSQL_RES *res = NULL;
    pthread_mutex_lock(db_lock);
    res = db_execute_query(db, sql);
    pthread_mutex_unlock(db_lock);
    
    if (res && mysql_num_rows(res) > 0) {
        mysql_free_result(res);
        cJSON_AddNumberToObject(root, "code", 400);
        cJSON_AddStringToObject(root, "msg", "Folder already exists");
        return root;
    }
    if (res) mysql_free_result(res);

    snprintf(sql, sizeof(sql), 
             "INSERT INTO files (user_id, file_name, file_size, file_path, file_type, parent_id) "
             "VALUES (%ld, '%s', 0, '/', 1, %d)", 
             user_id, esc_name, parent_id);

    pthread_mutex_lock(db_lock);
    if (db_execute_update(db, sql) > 0) {
        pthread_mutex_unlock(db_lock);
        cJSON_AddNumberToObject(root, "code", 0);
        cJSON_AddStringToObject(root, "msg", "Folder created successfully");
    } else {
        pthread_mutex_unlock(db_lock);
        cJSON_AddNumberToObject(root, "code", 500);
        cJSON_AddStringToObject(root, "msg", "Failed to create folder");
    }

    return root;
}

// 辅助：检查剩余配额
static int check_quota_enough(DBConnection *db, long user_id, long long file_size, pthread_mutex_t *db_lock) {
    char sql[2048];
    snprintf(sql, sizeof(sql), 
             "SELECT (total_quota - used_quota) as remaining FROM user_storage_quota WHERE user_id=%ld", user_id);
    
    MYSQL_RES *res = NULL;
    int enough = 0;

    pthread_mutex_lock(db_lock);
    res = db_execute_query(db, sql);
    pthread_mutex_unlock(db_lock);
    
    if (res) {
        if (mysql_num_rows(res) > 0) {
            MYSQL_ROW row = mysql_fetch_row(res);
            long long remaining = atoll(row[0]);
            if (remaining >= file_size) enough = 1;
        }
        mysql_free_result(res);
    }
    return enough;
}

cJSON* handle_upload_check(DBConnection *db, long user_id, const cJSON *req_json, pthread_mutex_t *db_lock) {
    if (!req_json) {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "code", 500);
        cJSON_AddStringToObject(root, "msg", "Invalid JSON Request");
        return root;
    }

    cJSON *root = cJSON_CreateObject();
    
    const char *md5 = cJSON_GetObjectItem(req_json, "md5")->valuestring;
    long long file_size = cJSON_GetObjectItem(req_json, "file_size")->valueint;
    const char *file_name = cJSON_GetObjectItem(req_json, "file_name")->valuestring;
    int parent_id = cJSON_GetObjectItem(req_json, "parent_id")->valueint;

    if (!md5 || file_size <= 0) {
        cJSON_AddNumberToObject(root, "code", 400);
        cJSON_AddStringToObject(root, "msg", "Invalid parameters");
        return root;
    }

    // 检查配额
    int quota_status = check_quota_enough(db, user_id, file_size, db_lock);
    if (quota_status == 0) {
        cJSON_AddNumberToObject(root, "code", 402);
        cJSON_AddStringToObject(root, "msg", "Storage quota exceeded");
        return root;
    }

    // 秒传检测：查询 files 表是否存在该 MD5
    char sql[2048];
    char existing_path[512];
    
    snprintf(sql, sizeof(sql), "SELECT file_path FROM files WHERE md5='%s'", md5);
    MYSQL_RES *res = NULL;
    
    pthread_mutex_lock(db_lock);
    res = db_execute_query(db, sql);
    pthread_mutex_unlock(db_lock);
    
    if (res && mysql_num_rows(res) > 0) {
        MYSQL_ROW row = mysql_fetch_row(res);
        
        // 拷贝路径
        strncpy(existing_path, row[0], sizeof(existing_path));
        existing_path[sizeof(existing_path)-1] = '\0';
        
        mysql_free_result(res);

        char esc_name[256];
        db_escape_string(db, esc_name, file_name, strlen(file_name));
        
        snprintf(sql, sizeof(sql), 
                 "INSERT INTO files (user_id, file_name, file_size, file_path, file_type, parent_id, md5) "
                 "VALUES (%ld, '%s', %lld, '%s', 0, %d, '%s')", 
                 user_id, esc_name, file_size, existing_path, parent_id, md5);
        
        pthread_mutex_lock(db_lock);
        if (db_execute_update(db, sql) > 0) {
            pthread_mutex_unlock(db_lock);
            snprintf(sql, sizeof(sql), "UPDATE user_storage_quota SET used_quota = used_quota + %lld WHERE user_id=%ld", file_size, user_id);
            db_execute_update(db, sql);
            cJSON_AddNumberToObject(root, "code", 0);
            cJSON_AddStringToObject(root, "msg", "Instant upload success");
            cJSON *data = cJSON_CreateObject();
            cJSON_AddStringToObject(data, "status", "instant");
            cJSON_AddItemToObject(root, "data", data);
            return root;
        } else {
            pthread_mutex_unlock(db_lock);
            cJSON_AddNumberToObject(root, "code", 500);
            cJSON_AddStringToObject(root, "msg", "DB error during instant upload");
            return root;
        }
    }
    if (res) mysql_free_result(res);

    // 断点续传检测
    char record_id_str[64];
    char offset_str[64];
    
    snprintf(sql, sizeof(sql), 
             "SELECT record_id, offset, status FROM file_records WHERE user_id=%ld AND file_md5='%s'", 
             user_id, md5);
    
    pthread_mutex_lock(db_lock);
    res = db_execute_query(db, sql);
    pthread_mutex_unlock(db_lock);

    if (res && mysql_num_rows(res) > 0) {
        MYSQL_ROW row = mysql_fetch_row(res);
        
        strcpy(record_id_str, row[0]);
        strcpy(offset_str, row[1]);
        int status = atoi(row[2]);
        
        mysql_free_result(res);

        if (status == 0) {
            cJSON_AddNumberToObject(root, "code", 0);
            cJSON_AddStringToObject(root, "msg", "Resume upload");
            cJSON *data = cJSON_CreateObject();
            cJSON_AddStringToObject(data, "status", "resume");
            cJSON_AddNumberToObject(data, "offset", atoll(offset_str));
            cJSON_AddItemToObject(root, "data", data);
            return root;
        }
    }
    // 【关键修复】使用 else if 避免双重释放
    else if (res) mysql_free_result(res);

    // 普通上传
    char temp_path[512];
    snprintf(temp_path, sizeof(temp_path), "/storage/upload_%ld_%s.tmp", user_id, md5);

    snprintf(sql, sizeof(sql), 
             "INSERT INTO file_records (user_id, file_md5, file_size, file_path, status, offset) "
             "VALUES (%ld, '%s', %lld, '%s', 0, 0)", 
             user_id, md5, file_size, temp_path);
    
    pthread_mutex_lock(db_lock);
    if (db_execute_update(db, sql) > 0) {
        pthread_mutex_unlock(db_lock);
        cJSON_AddNumberToObject(root, "code", 0);
        cJSON_AddStringToObject(root, "msg", "Ready to upload");
        cJSON *data = cJSON_CreateObject();
        cJSON_AddStringToObject(data, "status", "new");
        cJSON_AddNumberToObject(data, "offset", 0);
        cJSON_AddItemToObject(root, "data", data);
    } else {
        pthread_mutex_unlock(db_lock);
        cJSON_AddNumberToObject(root, "code", 500);
        cJSON_AddStringToObject(root, "msg", "Failed to create upload record");
    }
    return root;
}

void handle_upload_chunk(int client_fd, DBConnection *db, long user_id, HttpRequest *req, pthread_mutex_t *db_lock) {
    char md5[64] = {0};
    char offset_str[32] = {0};
    get_url_param(req->url, "md5", md5);
    get_url_param(req->url, "offset", offset_str);
    int offset = atoi(offset_str);

    const int CHUNK_SIZE_C = 5 * 1024 * 1024;
    char filepath_real[512]; 
    char sql[2048];

    // 查询路径
    pthread_mutex_lock(db_lock);
    snprintf(sql, sizeof(sql), 
             "SELECT file_path FROM file_records WHERE user_id=%ld AND file_md5='%s' AND status=0", 
             user_id, md5);
    
    MYSQL_RES *res = db_execute_query(db, sql);
    if (!res || mysql_num_rows(res) == 0) {
        if(res) mysql_free_result(res);
        pthread_mutex_unlock(db_lock);
        const char *not_found = "HTTP/1.1 404 Not Found\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: 0\r\n\r\n";
        send(client_fd, not_found, strlen(not_found), 0);
        close(client_fd);
        return;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    strncpy(filepath_real, row[0], sizeof(filepath_real));
    filepath_real[sizeof(filepath_real)-1] = '\0';
    
    mysql_free_result(res);
    pthread_mutex_unlock(db_lock);

    if (!req->body || req->body_len == 0) {
        const char *err_header = "HTTP/1.1 400 Bad Request\r\nAccess-Control-Allow-Origin: *\r\n\r\n";
        send(client_fd, err_header, strlen(err_header), 0);
        close(client_fd);
        return;
    }

    const char *dir_path = "/storage";
    struct stat st = {0};
    if (stat(dir_path, &st) == -1) {
        if (mkdir(dir_path, 0777) == -1) {
            const char *err_header = "HTTP/1.1 500 Server Error\r\nAccess-Control-Allow-Origin: *\r\n\r\n";
            send(client_fd, err_header, strlen(err_header), 0);
            close(client_fd);
            return;
        }
    }

    int fd = open(filepath_real, O_WRONLY | O_CREAT, 0644);
    if (fd < 0) {
        const char *err_header = "HTTP/1.1 500 Internal Error\r\nAccess-Control-Allow-Origin: *\r\n\r\n";
        send(client_fd, err_header, strlen(err_header), 0);
        close(client_fd);
        return;
    }

    ssize_t written = pwrite(fd, req->body, req->body_len, (off_t)offset);
    close(fd);

    pthread_mutex_lock(db_lock);
    if (written > 0) {
        int new_total_offset = offset + written;
        // 强制更新 DB
        snprintf(sql, sizeof(sql), 
                 "UPDATE file_records SET offset = %ld WHERE user_id=%ld AND file_md5='%s'", 
                 (long)new_total_offset, user_id, md5);
        db_execute_update(db, sql);
    }
    pthread_mutex_unlock(db_lock);

    const char *ok = "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\n\r\n";
    send(client_fd, ok, strlen(ok), 0);
    close(client_fd);
}

cJSON* handle_upload_complete(DBConnection *db, long user_id, const cJSON *req_json, pthread_mutex_t *db_lock) {
    cJSON *root = cJSON_CreateObject();
    
    cJSON *md5_item = cJSON_GetObjectItem(req_json, "md5");
    if (!md5_item || !md5_item->valuestring) {
        cJSON_AddNumberToObject(root, "code", 400);
        cJSON_AddStringToObject(root, "msg", "Missing md5");
        return root;
    }
    const char *md5 = md5_item->valuestring;

    cJSON *name_item = cJSON_GetObjectItem(req_json, "file_name");
    cJSON *pid_item = cJSON_GetObjectItem(req_json, "parent_id");
    
    if (!name_item || !name_item->valuestring) {
        cJSON_AddNumberToObject(root, "code", 400);
        cJSON_AddStringToObject(root, "msg", "Missing file_name");
        return root;
    }
    const char *file_name = name_item->valuestring;
    int parent_id = pid_item ? pid_item->valueint : 0;

    char sql[2048];
    char final_path_real[512]; // 本地缓冲区

    // 查询上传记录
    snprintf(sql, sizeof(sql), 
             "SELECT file_size, file_path FROM file_records WHERE user_id=%ld AND file_md5='%s' AND status=0", 
             user_id, md5);
    
    MYSQL_RES *res = NULL;
    pthread_mutex_lock(db_lock);
    res = db_execute_query(db, sql);
    pthread_mutex_unlock(db_lock);
    
    if (!res || mysql_num_rows(res) == 0) {
        if(res) mysql_free_result(res);
        cJSON_AddNumberToObject(root, "code", 404);
        cJSON_AddStringToObject(root, "msg", "Upload record not found");
        return root;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    long long file_size = atoll(row[0]);
    
    // 拷贝路径
    strncpy(final_path_real, row[1], sizeof(final_path_real));
    final_path_real[sizeof(final_path_real)-1] = '\0';
    
    mysql_free_result(res);

    char esc_name[256];
    db_escape_string(db, esc_name, file_name, strlen(file_name));

    // 插入文件信息 (必须成功)
    snprintf(sql, sizeof(sql), 
             "INSERT INTO files (user_id, file_name, file_size, file_path, file_type, parent_id, md5) "
             "VALUES (%ld, '%s', %lld, '%s', 0, %d, '%s')", 
             user_id, esc_name, file_size, final_path_real, parent_id, md5);

    pthread_mutex_lock(db_lock);
    int insert_ret = db_execute_update(db, sql);
    if (insert_ret <= 0) {
        pthread_mutex_unlock(db_lock);
        cJSON_AddNumberToObject(root, "code", 500);
        cJSON_AddStringToObject(root, "msg", "Failed to save file metadata");
        return root;
    }
    pthread_mutex_unlock(db_lock);

    snprintf(sql, sizeof(sql), "UPDATE file_records SET status=1 WHERE user_id=%ld AND file_md5='%s'", user_id, md5);
    pthread_mutex_lock(db_lock);
    db_execute_update(db, sql);
    pthread_mutex_unlock(db_lock);

    snprintf(sql, sizeof(sql), "UPDATE user_storage_quota SET used_quota = used_quota + %lld WHERE user_id=%ld", file_size, user_id);
    pthread_mutex_lock(db_lock);
    db_execute_update(db, sql);
    pthread_mutex_unlock(db_lock);

    snprintf(sql, sizeof(sql), 
             "INSERT INTO audit_logs (user_id, action_type, detail) VALUES (%ld, 'UPLOAD', 'File %s uploaded')", 
             user_id, esc_name);
    pthread_mutex_lock(db_lock);
    db_execute_update(db, sql);
    pthread_mutex_unlock(db_lock);

    cJSON_AddNumberToObject(root, "code", 0);
    cJSON_AddStringToObject(root, "msg", "Upload complete");
    return root;
}

void handle_file_download(int client_fd, DBConnection *db, long user_id, const cJSON *req_json, pthread_mutex_t *db_lock) {
    cJSON *id_obj = cJSON_GetObjectItem(req_json, "file_id");
    if (!id_obj) {
        const char *err = "HTTP/1.1 400 Bad Request\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: 0\r\n\r\n";
        send(client_fd, err, strlen(err), 0);
        close(client_fd);
        return;
    }
    long file_id = id_obj->valueint;

    char sql[2048]; // 【修复】增大缓冲区
    char filename_real[256]; 
    char filepath_real[512];

    snprintf(sql, sizeof(sql), "SELECT file_name, file_path, file_size FROM files WHERE file_id=%ld AND user_id=%ld", file_id, user_id);
    
    MYSQL_RES *res = NULL;
    pthread_mutex_lock(db_lock);
    res = db_execute_query(db, sql);
    pthread_mutex_unlock(db_lock);
    
    if (!res || mysql_num_rows(res) == 0) {
        if(res) mysql_free_result(res);
        const char *not_found = "HTTP/1.1 404 Not Found\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: 0\r\n\r\n";
        send(client_fd, not_found, strlen(not_found), 0);
        close(client_fd);
        return;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    
    strncpy(filename_real, row[0], sizeof(filename_real));
    filename_real[sizeof(filename_real)-1] = '\0';
    
    strncpy(filepath_real, row[1], sizeof(filepath_real));
    filepath_real[sizeof(filepath_real)-1] = '\0';
    
    off_t file_size = atoll(row[2]); 

    mysql_free_result(res);

    int file_fd = open(filepath_real, O_RDONLY);
    if (file_fd < 0) {
        const char *server_err = "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: 0\r\n\r\n";
        send(client_fd, server_err, strlen(server_err), 0);
        close(client_fd);
        return;
    }

    char header[1024];
    snprintf(header, sizeof(header), 
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: application/octet-stream\r\n"
             "Content-Disposition: attachment; filename=\"%s\"\r\n"
             "Content-Length: %ld\r\n"
             "Access-Control-Allow-Origin: *\r\n"
             "Connection: close\r\n"
             "\r\n", filename_real, (long)file_size);
    
    send(client_fd, header, strlen(header), 0);
    sendfile(client_fd, file_fd, NULL, file_size);

    close(file_fd);
    close(client_fd);
}

cJSON* handle_file_rename(DBConnection *db, long user_id, const cJSON *req_json, pthread_mutex_t *db_lock) {
    cJSON *root = cJSON_CreateObject();
    cJSON *id_obj = cJSON_GetObjectItem(req_json, "file_id");
    cJSON *name_obj = cJSON_GetObjectItem(req_json, "new_name");

    if (!id_obj || !name_obj) {
        cJSON_AddNumberToObject(root, "code", 400);
        cJSON_AddStringToObject(root, "msg", "Missing parameters");
        return root;
    }

    long file_id = id_obj->valueint;
    const char *new_name = name_obj->valuestring;

    char sql[2048];
    char current_name[512];
    char esc_name[256];
    
    // 1. 查询信息
    snprintf(sql, sizeof(sql), "SELECT parent_id, file_type, file_name FROM files WHERE file_id=%ld AND user_id=%ld", file_id, user_id);
    MYSQL_RES *res = NULL;
    pthread_mutex_lock(db_lock);
    res = db_execute_query(db, sql);
    pthread_mutex_unlock(db_lock);
    
    if (!res || mysql_num_rows(res) == 0) {
        if(res) mysql_free_result(res);
        cJSON_AddNumberToObject(root, "code", 404);
        cJSON_AddStringToObject(root, "msg", "File not found");
        return root;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    int parent_id = atoi(row[0]);
    mysql_free_result(res);

    // 2. 检查同名
    db_escape_string(db, esc_name, new_name, strlen(new_name));
    snprintf(sql, sizeof(sql), 
             "SELECT file_id FROM files WHERE user_id=%ld AND parent_id=%d AND file_name='%s'", 
             user_id, parent_id, esc_name);
    res = db_execute_query(db, sql);
    if (res && mysql_num_rows(res) > 0) {
        mysql_free_result(res);
        cJSON_AddNumberToObject(root, "code", 400);
        cJSON_AddStringToObject(root, "msg", "Name already exists");
        return root;
    }
    if(res) mysql_free_result(res);

    snprintf(sql, sizeof(sql), "UPDATE files SET file_name='%s' WHERE file_id=%ld", esc_name, file_id);
    pthread_mutex_lock(db_lock);
    if (db_execute_update(db, sql) > 0) {
        pthread_mutex_unlock(db_lock);
        cJSON_AddNumberToObject(root, "code", 0);
        cJSON_AddStringToObject(root, "msg", "Rename success");
    } else {
        pthread_mutex_unlock(db_lock);
        cJSON_AddNumberToObject(root, "code", 500);
        cJSON_AddStringToObject(root, "msg", "Database error");
    }

    return root;
}

cJSON* handle_file_delete(DBConnection *db, long user_id, const cJSON *req_json, pthread_mutex_t *db_lock) {
    cJSON *root = cJSON_CreateObject();
    cJSON *id_obj = cJSON_GetObjectItem(req_json, "file_id");
    if (!id_obj) {
        cJSON_AddNumberToObject(root, "code", 400);
        cJSON_AddStringToObject(root, "msg", "Missing file_id");
        return root;
    }
    long file_id = id_obj->valueint;

    char sql[2048];
    char filepath_real[512]; 
    long long file_size = 0;
    int file_type = 0;

    // 1. 查询信息
    snprintf(sql, sizeof(sql), 
             "SELECT file_path, file_size, file_type FROM files WHERE file_id=%ld AND user_id=%ld", 
             file_id, user_id);

    MYSQL_RES *res = NULL;
    pthread_mutex_lock(db_lock);
    res = db_execute_query(db, sql);
    pthread_mutex_unlock(db_lock);
    
    if (!res || mysql_num_rows(res) == 0) {
        if(res) mysql_free_result(res);
        cJSON_AddNumberToObject(root, "code", 404);
        cJSON_AddStringToObject(root, "msg", "File not found");
        return root;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    strncpy(filepath_real, row[0], sizeof(filepath_real));
    filepath_real[sizeof(filepath_real)-1] = '\0';
    file_size = atoll(row[1]);
    file_type = atoi(row[2]);
    mysql_free_result(res);

    // 2. 检查文件夹是否为空
    if (file_type == 1) {
        snprintf(sql, sizeof(sql), "SELECT count(*) FROM files WHERE parent_id=%ld", file_id);
        pthread_mutex_lock(db_lock);
        res = db_execute_query(db, sql);
        pthread_mutex_unlock(db_lock);
        if (res) {
            MYSQL_ROW count_row = mysql_fetch_row(res);
            if (atoi(count_row[0]) > 0) {
                mysql_free_result(res);
                cJSON_AddNumberToObject(root, "code", 400);
                cJSON_AddStringToObject(root, "msg", "Folder is not empty");
                return root;
            }
            mysql_free_result(res);
        }
    }

    // 3. 删除物理文件
    if (file_type == 0) {
        if (unlink(filepath_real) != 0) {
            perror("Delete physical file failed");
        }
    }

    // 4. 删除数据库记录
    snprintf(sql, sizeof(sql), "DELETE FROM files WHERE file_id=%ld", file_id);
    pthread_mutex_lock(db_lock);
    if (db_execute_update(db, sql) > 0) {
        pthread_mutex_unlock(db_lock);
        // 5. 扣减配额
        if (file_type == 0) {
            snprintf(sql, sizeof(sql), "UPDATE user_storage_quota SET used_quota = used_quota - %lld WHERE user_id=%ld", file_size, user_id);
            db_execute_update(db, sql);
        }
        cJSON_AddNumberToObject(root, "code", 0);
        cJSON_AddStringToObject(root, "msg", "Delete success");
    } else {
        pthread_mutex_unlock(db_lock);
        cJSON_AddNumberToObject(root, "code", 500);
        cJSON_AddStringToObject(root, "msg", "Database error");
    }

    return root;
}

cJSON* handle_file_move(DBConnection *db, long user_id, const cJSON *req_json, pthread_mutex_t *db_lock) {
    cJSON *root = cJSON_CreateObject();
    cJSON *id_obj = cJSON_GetObjectItem(req_json, "file_id");
    cJSON *target_id_obj = cJSON_GetObjectItem(req_json, "target_parent_id");

    if (!id_obj || !target_id_obj) {
        cJSON_AddNumberToObject(root, "code", 400);
        cJSON_AddStringToObject(root, "msg", "Missing parameters");
        return root;
    }

    long file_id = id_obj->valueint;
    long target_parent_id = target_id_obj->valueint;

    char sql[2048];
    char current_name[256];

    // 1. 检查目标文件夹
    if (target_parent_id != 0) {
        snprintf(sql, sizeof(sql), 
                 "SELECT file_id FROM files WHERE file_id=%ld AND user_id=%ld AND file_type=1", 
                 target_parent_id, user_id);
        MYSQL_RES *res = NULL;
        pthread_mutex_lock(db_lock);
        res = db_execute_query(db, sql);
        pthread_mutex_unlock(db_lock);
        
        if (!res || mysql_num_rows(res) == 0) {
            if(res) mysql_free_result(res);
            cJSON_AddNumberToObject(root, "code", 404);
            cJSON_AddStringToObject(root, "msg", "Target folder not found");
            return root;
        }
        mysql_free_result(res);
    }

    // 2. 检查源文件 & 目标同名
    snprintf(sql, sizeof(sql), "SELECT file_name FROM files WHERE file_id=%ld", file_id);
    MYSQL_RES *res = db_execute_query(db, sql);
    if (!res || mysql_num_rows(res) == 0) {
        if(res) mysql_free_result(res);
        cJSON_AddNumberToObject(root, "code", 404);
        cJSON_AddStringToObject(root, "msg", "Source file not found");
        return root;
    }
    
    MYSQL_ROW row = mysql_fetch_row(res);
    strncpy(current_name, row[0], sizeof(current_name));
    current_name[sizeof(current_name)-1] = '\0';
    mysql_free_result(res);

    char esc_name[256];
    db_escape_string(db, esc_name, current_name, strlen(current_name));

    snprintf(sql, sizeof(sql), 
             "SELECT file_id FROM files WHERE user_id=%ld AND parent_id=%ld AND file_name='%s'", 
             user_id, target_parent_id, esc_name);
    res = db_execute_query(db, sql);
    if (res && mysql_num_rows(res) > 0) {
        mysql_free_result(res);
        cJSON_AddNumberToObject(root, "code", 400);
        cJSON_AddStringToObject(root, "msg", "Target already contains a file/folder with this name");
        return root;
    }
    if(res) mysql_free_result(res);

    // 3. 执行移动
    snprintf(sql, sizeof(sql), "UPDATE files SET parent_id=%ld WHERE file_id=%ld", target_parent_id, file_id);
    pthread_mutex_lock(db_lock);
    if (db_execute_update(db, sql) > 0) {
        pthread_mutex_unlock(db_lock);
        cJSON_AddNumberToObject(root, "code", 0);
        cJSON_AddStringToObject(root, "msg", "Move success");
    } else {
        pthread_mutex_unlock(db_lock);
        cJSON_AddNumberToObject(root, "code", 500);
        cJSON_AddStringToObject(root, "msg", "Database error");
    }

    return root;
}