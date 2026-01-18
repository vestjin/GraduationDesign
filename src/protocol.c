#include "../include/protocol.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static HttpMethod get_method_type(const char *method) {
    if (strcmp(method, "GET") == 0) return HTTP_GET;
    if (strcmp(method, "POST") == 0) return HTTP_POST;
    if (strcmp(method, "PUT") == 0) return HTTP_PUT;
    if (strcmp(method, "DELETE") == 0) return HTTP_DELETE;
    if (strcmp(method, "OPTIONS") == 0) return HTTP_OPTIONS; // 支持 OPTIONS
    return HTTP_UNKNOWN;
}

int parse_http_request(const char *buffer, int size, HttpRequest *req) {
    memset(req, 0, sizeof(HttpRequest));

    // 1. 解析请求行
    char *line_end = strstr(buffer, "\r\n");
    if (!line_end) return -1; // 头部太短

    char method[16] = {0};
    const char *start = buffer;
    const char *space = strchr(start, ' ');
    if (!space || space - start > 15) return -1;
    strncpy(method, start, space - start);
    req->method = get_method_type(method);

    start = space + 1;
    space = strchr(start, ' ');
    if (!space || space - start > 255) return -1;
    strncpy(req->url, start, space - start);
    req->url[space - start] = '\0'; // 【修复】确保 URL 结束符

    // 2. 解析 Headers (寻找 Content-Length)
    char *body_start = strstr(buffer, "\r\n\r\n");
    if (!body_start) return -1; // 缺少头部结束符，格式错误

    char *len_ptr = strstr(buffer, "Content-Length:");
    int content_length = 0;
    if (len_ptr) {
        sscanf(len_ptr, "Content-Length: %d", &content_length);
    }

    // 3. 提取 Body (仅在 Content-Length > 0 时分配)
    // 计算 body 指针
    const char *body_ptr = body_start + 4;
    // 计算剩余大小
    int body_len = size - (body_ptr - buffer);

    if (content_length > 0) {
        // 安全检查：防止声明长度比实际收到的还大
        if (content_length > body_len) {
            // 这是一个截断的包或攻击
            // 我们只拷贝能拷贝的
            content_length = body_len;
        }

        req->body = (char*)malloc(content_length + 1);
        if (!req->body) return -1;
        
        memcpy(req->body, body_ptr, content_length);
        req->body[content_length] = '\0';
        req->body_len = content_length;
    } else {
        req->body = NULL;
        req->body_len = 0;
    }

    // 解析 Headers Token 和 File-Offset
    if (strstr(buffer, "Token:")) {
        sscanf(strstr(buffer, "Token:"), "Token: %127s", req->token);
    }
    if (strstr(buffer, "File-Offset:")) {
        sscanf(strstr(buffer, "File-Offset:"), "File-Offset: %d", &req->file_offset);
    }

    return 0;
}

void free_http_request(HttpRequest *req) {
    if (req->body) {
        free(req->body);
        req->body = NULL;
    }
}