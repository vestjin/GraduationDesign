#include "../include/server.h"
#include "../include/thread_pool.h"
#include "../include/conf.h"
#include "../include/db_op.h"
#include "../include/protocol.h"
#include "../include/api_util.h"
#include "../include/user_handler.h"
#include "../include/file_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>
#include <pthread.h>

#define MAX_EVENTS 64
#define BUFFER_SIZE (7 * 1024 * 1024) // 7MB 缓冲区

// 全局变量
extern Config g_conf;
extern DBConnection *g_db;
extern pthread_mutex_t g_db_lock;

static int set_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
}

// 【新增】设置 Socket 超时
// 防止线程永远阻塞在 recv 上 (例如遇到 Expect: 100-continue 死锁)
static int set_socket_timeout(int sockfd) {
    struct timeval tv;
    tv.tv_sec = 10;  // 10秒超时
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0) {
        perror("setsockopt timeout");
        return -1;
    }
    return 0;
}
// 处理客户端请求
void process_client_request(void *arg) {
    int client_fd = *(int*)arg;
    free(arg); 
    // 1. 分配内存 (减小了 Size)
    char *buffer = malloc(BUFFER_SIZE);
    if (!buffer) {
        printf("MALLOC FAILED!\n"); // 打印日志排查
        close(client_fd);
        return;
    }
    // 【关键】增加超时设置
    if (set_socket_timeout(client_fd) < 0) {
        free(buffer);
        close(client_fd);
        return;
    }

    // 2. 设置阻塞模式
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags & ~O_NONBLOCK);

    int total_read = 0;
    int content_length = 0;
    int header_found = 0;
    int header_len = 0;

    // 3. 循环读取 Header
    while (total_read < BUFFER_SIZE - 1) {
        int bytes = recv(client_fd, buffer + total_read, (BUFFER_SIZE - 1) - total_read, 0);
        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 超时了？正常来说不会 EAGAIN，因为我们已经设置了 SO_RCVTIMEO
                // 如果这里触发了，说明超时了
                printf("Timeout or Error\n");
            } else {
                perror("recv error");
            }
            goto cleanup;
        }
        if (bytes == 0) {
            printf("Client closed connection\n");
            goto cleanup;
        }
        total_read += bytes;
        buffer[total_read] = '\0'; 

        char *body_start = strstr(buffer, "\r\n\r\n");
        if (body_start) {
            header_len = body_start - buffer + 4; 
            char *len_ptr = strstr(buffer, "Content-Length:");
            if (len_ptr) {
                sscanf(len_ptr, "Content-Length: %d", &content_length);
            }
            header_found = 1;
            break; 
        }
    }

    // 4. 循环读取 Body
    if (header_found) {
        int total_needed = header_len + content_length;
        // 【安全检查】防止 Content-Length 声称的数值过大，导致 Buffer 溢出
        if (total_needed > BUFFER_SIZE) {
            printf("Request too large (declared %d, buffer %d)\n", total_needed, BUFFER_SIZE);
            goto cleanup;
        }

        while (total_read < total_needed) {
            int bytes = recv(client_fd, buffer + total_read, total_needed - total_read, 0);
            if (bytes < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                     printf("Timeout reading body\n");
                } else {
                    perror("recv body error");
                }
                goto cleanup;
            }
            if (bytes == 0) {
                printf("Client disconnected during body transfer\n");
                goto cleanup;
            }
            total_read += bytes;
        }
    } else {
        printf("Header error\n");
        goto cleanup;
    }

    // 5. 解析 HTTP
    HttpRequest req;
    if (parse_http_request(buffer, total_read, &req) != 0) {
        const char *bad_resp = "HTTP/1.1 400 Bad Request\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: 0\r\n\r\n";
        send(client_fd, bad_resp, strlen(bad_resp), 0);
        goto cleanup;
    }

    printf("[Thread %lu] Handling: %s %s\n", pthread_self(), 
           (req.method==HTTP_GET?"GET":"POST"), req.url);

    
    // 【关键修复】全局解析 JSON，并确保非 NULL
    // 即使没有 body，也创建一个空对象，方便 file_handler 使用 cJSON_GetObjectItem
    cJSON *req_json = NULL;
    if (req.method == HTTP_POST && req.body_len > 0) {
        req_json = cJSON_Parse(req.body);
    }
    
    // 如果解析失败（例如 Body 不是 JSON），也创建一个空对象，防止后续函数访问空指针
    if (!req_json) {
        req_json = cJSON_CreateObject();
    }
           // 移除外层全局锁
           // 之前在这里枷锁，导致api串行执行
           // 现在改为在具体处理函数内枷锁
    // 6. 处理 OPTIONS (CORS 预检)
    if (req.method == HTTP_OPTIONS) {
        const char *cors_preflight = 
            "HTTP/1.1 200 OK\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type, Token\r\n"
            "\r\n";
        send(client_fd, cors_preflight, strlen(cors_preflight), 0);
        goto cleanup; 
    }

    cJSON *json_resp_obj = NULL; 

    // --- 用户模块 ---
    if (strcmp(req.url, "/api/user/login") == 0 && req.method == HTTP_POST) {
        // 这里的 req_json 保证不为 NULL
        json_resp_obj = handle_login(g_db, req_json, "127.0.0.1");
    } 
    else if (strcmp(req.url, "/api/user/register") == 0 && req.method == HTTP_POST) {
        json_resp_obj = handle_register(g_db, req_json);
    }

    // --- 文件模块 ---
    else if (strncmp(req.url, "/api/files/", 11) == 0) {
        // 验证 Token
        long user_id = verify_user_token(g_db, req.token, &g_db_lock);
        
        if (user_id <= 0) {
            json_resp_obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(json_resp_obj, "code", 401);
            cJSON_AddStringToObject(json_resp_obj, "msg", "Invalid or expired token");
        } else {
            // 文件操作使用同一个 req_json
            if (strcmp(req.url, "/api/files/list") == 0 && req.method == HTTP_POST) {
                json_resp_obj = handle_file_list(g_db, user_id, req_json, &g_db_lock);
            }
            else if (strcmp(req.url, "/api/files/mkdir") == 0 && req.method == HTTP_POST) {
                json_resp_obj = handle_file_mkdir(g_db, user_id, req_json, &g_db_lock);
            }
            else if (strcmp(req.url, "/api/files/rename") == 0 && req.method == HTTP_POST) {
                json_resp_obj = handle_file_rename(g_db, user_id, req_json, &g_db_lock);
            }
            else if (strcmp(req.url, "/api/files/delete") == 0 && req.method == HTTP_POST) {
                json_resp_obj = handle_file_delete(g_db, user_id, req_json, &g_db_lock);
            }
            else if (strcmp(req.url, "/api/files/move") == 0 && req.method == HTTP_POST) {
                json_resp_obj = handle_file_move(g_db, user_id, req_json, &g_db_lock);
            }
            else if (strcmp(req.url, "/api/files/upload/check") == 0 && req.method == HTTP_POST) {
                json_resp_obj = handle_upload_check(g_db, user_id, req_json, &g_db_lock);
            }
            else if (strcmp(req.url, "/api/files/upload/complete") == 0 && req.method == HTTP_POST) {
                json_resp_obj = handle_upload_complete(g_db, user_id, req_json, &g_db_lock);
            }
            else if (strcmp(req.url, "/api/files/download") == 0 && req.method == HTTP_POST) {
                handle_file_download(client_fd, g_db, user_id, req_json, &g_db_lock);
                json_resp_obj = (cJSON*)0x1; 
            }
            else if (strncmp(req.url, "/api/files/upload/chunk", 23) == 0 && req.method == HTTP_POST) {
                // 注意：这里不传 req_json，而是传 &req
                handle_upload_chunk(client_fd, g_db, user_id, &req, &g_db_lock);
                json_resp_obj = (cJSON*)0x1; 
            }
            else {
                json_resp_obj = cJSON_CreateObject();
                cJSON_AddNumberToObject(json_resp_obj, "code", 404);
                cJSON_AddStringToObject(json_resp_obj, "msg", "File API Not Found");
            }
        }
    }
    else {
        json_resp_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(json_resp_obj, "code", 404);
        cJSON_AddStringToObject(json_resp_obj, "msg", "API Not Found");
    }

    // 8. 发送响应
    if (json_resp_obj == (cJSON*)0x1) {
        goto cleanup; // 清理资源
    }

    char *json_body = cJSON_PrintUnformatted(json_resp_obj); 
    cJSON_Delete(json_resp_obj); 

    if (json_body) {
        char header[512];
        int body_len = strlen(json_body);
        snprintf(header, sizeof(header), 
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %d\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Access-Control-Allow-Headers: Content-Type, Token\r\n"
                 "Connection: close\r\n"
                 "\r\n", body_len);
        
        send(client_fd, header, strlen(header), 0);
        send(client_fd, json_body, body_len, 0);
        free(json_body);
    }

    // 9. 统一清理标签
cleanup:
    free_http_request(&req);
    // 注意：这里我们手动释放 req_json，因为它是在函数内部 malloc/parse 的
    if (req_json) cJSON_Delete(req_json);
    free(buffer); 
    close(client_fd);
}

void start_server(int port, int thread_count) {
    threadpool_t *pool = threadpool_create(thread_count);
    printf("Thread pool created with %d threads.\n", thread_count);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(1);
    }

    if (listen(listen_fd, 128) < 0) {
        perror("listen");
        exit(1);
    }

    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1");
        exit(1);
    }

    struct epoll_event ev, events[MAX_EVENTS];
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) < 0) {
        perror("epoll_ctl");
        exit(1);
    }

    printf("Server started on port %d. Waiting for connections...\n", port);

    while (1) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == listen_fd) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
                if (client_fd < 0) {
                    perror("accept");
                    continue;
                }
                set_nonblocking(client_fd);

                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
                    perror("epoll_ctl: client");
                    close(client_fd);
                }
                printf("New connection: fd=%d\n", client_fd);
            } else {
                int client_fd = events[i].data.fd;
                
                int *pfd = (int*)malloc(sizeof(int));
                *pfd = client_fd;

                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);

                if (threadpool_add(pool, process_client_request, pfd) != 0) {
                    fprintf(stderr, "Thread pool full, dropping request.\n");
                    free(pfd);
                    close(client_fd);
                }
            }
        }
    }

    close(listen_fd);
    close(epoll_fd);
    threadpool_destroy(pool);
}