#include "../include/conf.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// 简单的 key=value 解析函数
static void parse_line(char *line, char *key, char *value) {
    char *p = strchr(line, '=');
    if (p) {
        *p = '\0';
        p++; // 跳过 '='
        // 去除 key 首尾空格
        while (*line == ' ') line++;
        int len = strlen(line);
        while (len > 0 && line[len-1] == ' ') { line[len-1] = '\0'; len--; }
        
        // 去除 value 首尾空格
        while (*p == ' ') p++;
        len = strlen(p);
        while (len > 0 && p[len-1] == ' ') { p[len-1] = '\0'; len--; }

        strcpy(key, line);
        strcpy(value, p);
    }
}

int load_config(const char *filename, Config *conf) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Open config file failed");
        return -1;
    }

    char line[256];
    char key[128], value[128];
    int in_db_section = 0;

    while (fgets(line, sizeof(line), fp)) {
        // 去除换行符
        line[strcspn(line, "\r\n")] = 0;
        
        // 跳过注释和空行
        if (line[0] == '#' || line[0] == '\0') continue;

        // 检查 Section
        if (line[0] == '[') {
            if (strstr(line, "[database]")) {
                in_db_section = 1;
            } else {
                in_db_section = 0;
            }
            continue;
        }

        if (in_db_section) {
            parse_line(line, key, value);
            if (strcmp(key, "db_host") == 0) strcpy(conf->db_host, value);
            else if (strcmp(key, "db_port") == 0) conf->db_port = atoi(value);
            else if (strcmp(key, "db_user") == 0) strcpy(conf->db_user, value);
            else if (strcmp(key, "db_pass") == 0) strcpy(conf->db_pass, value);
            else if (strcmp(key, "db_name") == 0) strcpy(conf->db_name, value);
        }
    }

    fclose(fp);
    return 0;
}