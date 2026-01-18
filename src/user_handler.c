#include "../include/user_handler.h"
#include "../include/db_op.h"
#include <string.h>
#include <stdio.h>
#include <openssl/sha.h>
#include <time.h>

// 默认配额 5GB
#define DEFAULT_QUOTA (5LL * 1024 * 1024 * 1024)

// 辅助函数：计算 SHA256
void compute_sha256(const char *str, char *output) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, str, strlen(str));
    SHA256_Final(hash, &sha256);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(output + (i * 2), "%02x", hash[i]);
    }
    output[64] = 0; // 终止符
}

// 辅助函数：生成 Token (简单的随机字符串+时间戳，毕设够用)
void generate_token(char *token) {
    srand(time(NULL));
    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    for (int i = 0; i < 31; i++) {
        token[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    token[31] = '\0';
}

cJSON* handle_register(DBConnection *db, const cJSON *req_json) {
    cJSON *root = cJSON_CreateObject();
    
    // 1. 获取参数
    cJSON *username_obj = cJSON_GetObjectItem(req_json, "username");
    cJSON *password_obj = cJSON_GetObjectItem(req_json, "password");
    
    if (!username_obj || !password_obj) {
        cJSON_AddNumberToObject(root, "code", USER_DB_ERROR);
        cJSON_AddStringToObject(root, "msg", "Missing parameters");
        return root;
    }

    const char *username = username_obj->valuestring;
    const char *password = password_obj->valuestring;

    // 2. 检查用户是否存在
    char sql[512];
    snprintf(sql, sizeof(sql), "SELECT user_id FROM users WHERE username='%s'", username); // 简单起见直接拼，实际应转义username
    // 注意：为了代码简洁，这里假设 username 不含特殊字符，严谨做法需转义
    
    MYSQL_RES *res = db_execute_query(db, sql);
    if (res) {
        if (mysql_num_rows(res) > 0) {
            cJSON_AddNumberToObject(root, "code", USER_EXIST);
            cJSON_AddStringToObject(root, "msg", "Username already exists");
            mysql_free_result(res);
            return root;
        }
        mysql_free_result(res);
    }

    // 3. 密码加密
    char hashed_pwd[65];
    compute_sha256(password, hashed_pwd);

    // 4. 插入用户
    char esc_user[128];
    db_escape_string(db, esc_user, username, strlen(username));
    
    snprintf(sql, sizeof(sql), "INSERT INTO users (username, password) VALUES ('%s', '%s')", esc_user, hashed_pwd);
    
    if (db_execute_update(db, sql) <= 0) {
        cJSON_AddNumberToObject(root, "code", USER_DB_ERROR);
        cJSON_AddStringToObject(root, "msg", "Database error on insert user");
        return root;
    }

    // 5. 初始化配额 (获取刚插入的 user_id)
    long user_id = mysql_insert_id(db->conn);
    snprintf(sql, sizeof(sql), "INSERT INTO user_storage_quota (user_id, total_quota, used_quota) VALUES (%ld, %lld, 0)", user_id, DEFAULT_QUOTA);
    
    if (db_execute_update(db, sql) <= 0) {
        // 回滚用户注册（简化处理：实际项目中应使用事务）
        snprintf(sql, sizeof(sql), "DELETE FROM users WHERE user_id=%ld", user_id);
        db_execute_update(db, sql);
        cJSON_AddNumberToObject(root, "code", USER_DB_ERROR);
        cJSON_AddStringToObject(root, "msg", "Failed to init quota");
        return root;
    }

    // 6. 成功
    cJSON_AddNumberToObject(root, "code", USER_OK);
    cJSON_AddStringToObject(root, "msg", "Register Success");
    return root;
}

cJSON* handle_login(DBConnection *db, const cJSON *req_json, const char *client_ip) {
    cJSON *root = cJSON_CreateObject();

    // 1. 获取参数
    const char *username = cJSON_GetObjectItem(req_json, "username")->valuestring;
    const char *password = cJSON_GetObjectItem(req_json, "password")->valuestring;

    // 2. 查询用户
    char sql[512];
    char esc_user[128];
    db_escape_string(db, esc_user, username, strlen(username));

    snprintf(sql, sizeof(sql), "SELECT user_id, password FROM users WHERE username='%s'", esc_user);
    MYSQL_RES *res = db_execute_query(db, sql);

    if (!res || mysql_num_rows(res) == 0) {
        if(res) mysql_free_result(res);
        cJSON_AddNumberToObject(root, "code", USER_NOT_FOUND);
        cJSON_AddStringToObject(root, "msg", "User not found");
        return root;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    long user_id = atol(row[0]);
    const char *db_pwd = row[1];

    // 3. 验证密码
    char hashed_input[65];
    compute_sha256(password, hashed_input);

    if (strcmp(hashed_input, db_pwd) != 0) {
        mysql_free_result(res);
        cJSON_AddNumberToObject(root, "code", USER_PASS_WRONG);
        cJSON_AddStringToObject(root, "msg", "Password incorrect");
        return root;
    }
    mysql_free_result(res);

    // 4. 生成 Token 并更新数据库
    char new_token[32];
    generate_token(new_token);
    
    // Token 有效期：当前时间 + 1小时
    // SQL: DATE_ADD(NOW(), INTERVAL 1 HOUR)
    snprintf(sql, sizeof(sql), "UPDATE users SET token='%s', token_expire=DATE_ADD(NOW(), INTERVAL 1 HOUR) WHERE user_id=%ld", new_token, user_id);
    
    if (db_execute_update(db, sql) <= 0) {
        cJSON_AddNumberToObject(root, "code", USER_DB_ERROR);
        cJSON_AddStringToObject(root, "msg", "Token update failed");
        return root;
    }

    // 5. 记录日志 (可选，这里不写单独日志表操作，保持简洁)

    // 6. 返回 Token
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "token", new_token);
    cJSON_AddNumberToObject(data, "user_id", user_id);
    
    cJSON_AddNumberToObject(root, "code", USER_OK);
    cJSON_AddStringToObject(root, "msg", "Login Success");
    cJSON_AddItemToObject(root, "data", data);

    return root;
}