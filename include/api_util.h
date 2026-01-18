#ifndef _API_UTIL_H
#define _API_UTIL_H

#include "../include/protocol.h"
#include "../libs/cJSON/cJSON.h"

// 构造标准 JSON 响应
// code: 业务状态码 (0 成功, 非0 失败)
// msg: 提示信息
// data: cJSON 数据对象 (如果传 NULL，则 data 字段为 null)
// 返回: JSON 字符串 (需 free)
char* make_json_response(int code, const char *msg, cJSON *data);

#endif