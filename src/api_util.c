#include "../include/api_util.h"
#include <stdlib.h>
#include <string.h>

char* make_json_response(int code, const char *msg, cJSON *data) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "code", code);
    cJSON_AddStringToObject(root, "msg", msg);
    
    if (data != NULL) {
        cJSON_AddItemToObject(root, "data", data);
    } else {
        cJSON_AddNullToObject(root, "data");
    }

    char *json_str = cJSON_PrintUnformatted(root); // 压缩输出，节省带宽
    cJSON_Delete(root);
    return json_str;
}