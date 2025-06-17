#ifndef __FATFS_UNICODE_HELPER_H__
#define __FATFS_UNICODE_HELPER_H__

#include "stdint.h"
#include "ff.h"

#ifdef __cplusplus
extern "C" {
#endif

// Unicode支持配置 # 功能开关和参数配置
#define UNICODE_SUPPORT_ENABLED 1           // 启用Unicode支持功能
#define MAX_FILENAME_LEN 255                 // 最大文件名长度
#define SAFE_FILENAME_LEN 64                 // 安全文件名长度
#define SHORT_DATETIME_LEN 11                // 短时间戳长度(含结束符)

// Unicode状态结构体 # 状态监控和统计
typedef struct {
    uint8_t initialized;                     // 初始化状态标志
    uint32_t convert_count;                  // 转换次数统计
    uint32_t error_count;                    // 错误次数统计
    uint8_t last_error;                      // 最后错误码
    uint32_t optimization_count;             // 优化次数统计
} unicode_status_t;

// 文件名优化配置 # 优化策略参数
typedef struct {
    uint8_t enable_prefix_optimization;     // 启用前缀优化
    uint8_t enable_timestamp_optimization;  // 启用时间戳优化
    uint8_t max_optimization_level;         // 最大优化级别
    uint16_t target_length;                 // 目标文件名长度
} filename_optimization_config_t;

// 核心接口函数声明 # 主要功能接口
void fatfs_unicode_helper_init(void);                                    // 初始化Unicode助手模块
void fatfs_unicode_helper_cleanup(void);                                 // 清理Unicode助手模块
uint8_t is_filename_too_long(const char* filename);                      // 检查文件名是否过长
unicode_status_t* get_unicode_status(void);                              // 获取Unicode状态信息
void reset_unicode_counters(void);                                       // 重置统计计数器

// 文件名优化接口声明 # 文件名处理功能
void optimize_filename(char* optimized, const char* original, uint16_t max_len); // 优化文件名长度
void generate_short_datetime_string(char* datetime_str);                 // 生成短时间戳字符串
uint8_t validate_filename_chars(const char* filename);                   // 验证文件名字符合法性
void get_filename_components(const char* filename, char* path, char* name, char* ext); // 分解文件名组件

// 配置管理接口声明 # 配置和设置功能
void set_optimization_config(const filename_optimization_config_t* config); // 设置优化配置
filename_optimization_config_t* get_optimization_config(void);           // 获取优化配置
void enable_unicode_debug(uint8_t enable);                               // 启用调试模式
void show_unicode_status(void);                                          // 显示详细状态信息

// 统计和监控接口声明 # 调试和监控功能
uint32_t get_convert_count(void);                                        // 获取转换次数
uint32_t get_error_count(void);                                          // 获取错误次数
uint32_t get_optimization_count(void);                                   // 获取优化次数
void log_unicode_operation(const char* operation, const char* details);  // 记录Unicode操作日志

// 内部工具函数声明 # 内部辅助功能（可选暴露）
uint16_t calculate_filename_length(const char* filename);                // 计算文件名长度
uint8_t is_ascii_only(const char* filename);                             // 检查是否仅包含ASCII字符
void sanitize_filename(char* filename);                                  // 清理文件名非法字符

#ifdef __cplusplus
}
#endif

#endif // __FATFS_UNICODE_HELPER_H__
