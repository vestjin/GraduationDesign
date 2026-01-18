#ifndef _PROTOCOL_H
#define _PROTOCOL_H

#include <stdbool.h>

// HTTP 方法枚举
typedef enum {
    HTTP_GET,
    HTTP_POST,
    HTTP_PUT,
    HTTP_DELETE,
    HTTP_OPTIONS,
    HTTP_UNKNOWN
} HttpMethod;

// HTTP 请求结构体
typedef struct {
    HttpMethod method;
    char url[256];      // 请求路径，如 /files/upload
    char *body;         // 请求体（动态分配内存，需释放）
    int body_len;
    // 可根据需要扩展 headers，例如 token, content-type
    char token[128];
    char content_type[128];
    int file_offset;   // 用于读取HTTP头中分片的偏移量
} HttpRequest;

// HTTP 响应状态码
typedef enum {
    HTTP_OK = 200,
    HTTP_BAD_REQUEST = 400,
    HTTP_UNAUTHORIZED = 401,
    HTTP_NOT_FOUND = 404,
    HTTP_INTERNAL_SERVER_ERROR = 500
} HttpStatus;

// HTTP 响应结构体
typedef struct {
    HttpStatus status_code;
    char *body;         // JSON 字符串（动态分配，需释放）
} HttpResponse;

// 解析 HTTP 请求字符串
// buffer: 接收到的 raw 数据
// req: 输出参数，存储解析结果
// 返回: 0 成功, -1 失败
int parse_http_request(const char *buffer, int size, HttpRequest *req);

// 释放 HttpRequest 中的动态内存
void free_http_request(HttpRequest *req);

// 释放 HttpResponse 中的动态内存
void free_http_response(HttpResponse *resp);

#endif