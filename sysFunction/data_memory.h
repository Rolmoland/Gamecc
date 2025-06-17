#ifndef __DATA_MEMORY_H__
#define __DATA_MEMORY_H__

#include "stdint.h"
#include "ff.h"  // 包含FATFS定义

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

// 日志存储配置常量 # 日志系统参数配置
#define LOG_DIR_NAME "0:/log"                // 日志目录名 # TF卡日志目录路径
#define LOG_FILENAME_MAX_LEN 32              // 日志文件名最大长度 # 日志文件名缓冲区大小
#define LOG_DATA_BUFFER_SIZE 128             // 日志数据缓冲区大小 # 单条日志记录缓冲区
#define LOG_BOOT_COUNT_FLASH_ADDR 0x001000   // 上电次数Flash存储地址 # 4KB偏移避免配置冲突

// 隐藏数据存储配置常量 # 隐藏数据参数配置
#define HIDEDATA_RECORDS_PER_FILE 10         // 每文件记录数 # 隐藏文件最大记录条数
#define HIDEDATA_DIR_NAME "0:/hideData"      // 隐藏目录名 # TF卡隐藏目录路径
#define HIDEDATA_FILENAME_MAX_LEN 64         // 文件名最大长度 # 隐藏文件名缓冲区大小
#define HIDEDATA_DATA_BUFFER_SIZE 64         // 数据缓冲区大小 # 隐藏记录缓冲区

// 全局变量声明 # 模块状态变量
extern uint8_t g_sample_record_count;       // 当前文件记录计数 # 文件内记录数计数器
extern uint8_t g_overlimit_record_count;    // 超限文件记录计数 # 超限文件内记录数计数器
extern uint8_t g_hidedata_record_count;     // 隐藏文件记录计数 # 隐藏文件内记录数计数器
extern FATFS g_sample_fs;                   // 文件系统对象 # 全局文件系统对象

// 主接口函数声明 # 对外接口
void save_sample_data(float voltage);       // 保存采样数据到TF卡 # 主存储接口函数
void save_overlimit_data(float voltage, float limit_value); // 保存超限数据到TF卡 # 超限数据存储接口
void save_hidedata(float voltage);          // 保存隐藏数据到TF卡 # 隐藏数据存储接口

// 日志系统接口函数声明 # 日志系统对外接口
void log_init(void);                        // 初始化日志系统 # 日志系统初始化函数
void log_write(const char* message);        // 写入日志消息 # 日志记录接口函数
void log_printf(const char* format, ...);   // 格式化日志输出 # 格式化日志接口函数

#ifdef __cplusplus
}
#endif

#endif

