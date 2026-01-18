#ifndef _CONF_H
#define _CONF_H

// 定义配置信息结构体
typedef struct {
    char db_host[64];
    int db_port;
    char db_user[64];
    char db_pass[64];
    char db_name[64];
} Config;

// 加载配置文件
int load_config(const char *filename, Config *conf);

#endif