#ifndef __DATA_MEMORY_H__
#define __DATA_MEMORY_H__

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

// 数据存储配置常量 # 存储参数配置
#define SAMPLE_RECORDS_PER_FILE 10          // 每文件记录数 # 单文件最大记录条数
#define SAMPLE_DIR_NAME "0:/sample"          // 存储目录名 # TF卡存储目录路径
#define SAMPLE_FILENAME_MAX_LEN 64           // 文件名最大长度 # 文件名缓冲区大小
#define SAMPLE_DATA_BUFFER_SIZE 64           // 数据缓冲区大小 # 单条记录缓冲区

// 超限数据存储配置常量 # 超限数据参数配置
#define OVERLIMIT_RECORDS_PER_FILE 10        // 每文件记录数 # 超限文件最大记录条数
#define OVERLIMIT_DIR_NAME "0:/overLimit"    // 超限目录名 # TF卡超限目录路径
#define OVERLIMIT_FILENAME_MAX_LEN 64        // 文件名最大长度 # 超限文件名缓冲区大小
#define OVERLIMIT_DATA_BUFFER_SIZE 80        // 数据缓冲区大小 # 超限记录缓冲区（需要更大空间）

// 全局变量声明 # 模块状态变量
extern uint8_t g_sample_record_count;       // 当前文件记录计数 # 文件内记录数计数器
extern uint8_t g_overlimit_record_count;    // 超限文件记录计数 # 超限文件内记录数计数器

// 主接口函数声明 # 对外接口
void save_sample_data(float voltage);       // 保存采样数据到TF卡 # 主存储接口函数
void save_overlimit_data(float voltage, float limit_value); // 保存超限数据到TF卡 # 超限数据存储接口

#ifdef __cplusplus
}
#endif

#endif

