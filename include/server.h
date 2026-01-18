#ifndef _SERVER_H
#define _SERVER_H

#include <sys/socket.h>
#include <netinet/in.h>

// 处理客户端请求的函数
// arg: 传递给线程的参数
void process_client_request(void *arg);
// 启动服务器
// port: 监听端口
// thread_count: 线程池线程数
void start_server(int port, int thread_count);

#endif