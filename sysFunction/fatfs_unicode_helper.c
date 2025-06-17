#include "fatfs_unicode_helper.h"
#include "mcu_cmic_gd32f470vet6.h"
#include "string.h"
#include "stdio.h"

#if UNICODE_SUPPORT_ENABLED

// 全局状态变量 # 模块状态管理
static unicode_status_t g_unicode_status = {0};
static filename_optimization_config_t g_optimization_config = {
    .enable_prefix_optimization = 1,
    .enable_timestamp_optimization = 1,
    .max_optimization_level = 3,
    .target_length = SAFE_FILENAME_LEN
};
static uint8_t g_debug_enabled = 0;

// 前缀优化映射表 # 文件名前缀缩短映射
typedef struct {
    const char* original;
    const char* optimized;
} prefix_mapping_t;

static const prefix_mapping_t g_prefix_mappings[] = {
    {"sampleData", "sample"},
    {"overLimit", "over"},
    {"hideData", "hide"},
    {"logData", "log"},
    {NULL, NULL}  // 结束标记
};

// 初始化Unicode助手模块 # 模块初始化
void fatfs_unicode_helper_init(void)
{
    my_printf(DEBUG_USART, "====unicode helper init====\r\n");
    
    // 初始化状态结构
    memset(&g_unicode_status, 0, sizeof(unicode_status_t));
    g_unicode_status.initialized = 1;
    
    // 设置默认优化配置
    g_optimization_config.enable_prefix_optimization = 1;
    g_optimization_config.enable_timestamp_optimization = 1;
    g_optimization_config.max_optimization_level = 3;
    g_optimization_config.target_length = SAFE_FILENAME_LEN;
    
    my_printf(DEBUG_USART, "unicode helper ready, max filename: %d\r\n", MAX_FILENAME_LEN);
}

// 清理Unicode助手模块 # 模块清理
void fatfs_unicode_helper_cleanup(void)
{
    if(g_unicode_status.initialized) {
        my_printf(DEBUG_USART, "unicode helper cleanup: converts=%lu, errors=%lu, optimizations=%lu\r\n",
                 g_unicode_status.convert_count, g_unicode_status.error_count, g_unicode_status.optimization_count);
        
        memset(&g_unicode_status, 0, sizeof(unicode_status_t));
        g_debug_enabled = 0;
    }
}

// 检查文件名是否过长 # 长度验证
uint8_t is_filename_too_long(const char* filename)
{
    if(!filename || !g_unicode_status.initialized) {
        return 0;
    }
    
    uint16_t len = strlen(filename);
    return (len > g_optimization_config.target_length) ? 1 : 0;
}

// 获取Unicode状态信息 # 状态查询
unicode_status_t* get_unicode_status(void)
{
    return &g_unicode_status;
}

// 重置统计计数器 # 计数器重置
void reset_unicode_counters(void)
{
    if(g_unicode_status.initialized) {
        g_unicode_status.convert_count = 0;
        g_unicode_status.error_count = 0;
        g_unicode_status.optimization_count = 0;
        g_unicode_status.last_error = 0;
        
        if(g_debug_enabled) {
            my_printf(DEBUG_USART, "unicode counters reset\r\n");
        }
    }
}

// 计算文件名长度 # 长度计算
uint16_t calculate_filename_length(const char* filename)
{
    if(!filename) {
        return 0;
    }
    return strlen(filename);
}

// 检查是否仅包含ASCII字符 # ASCII检查
uint8_t is_ascii_only(const char* filename)
{
    if(!filename) {
        return 1;
    }
    
    while(*filename) {
        if((unsigned char)*filename > 127) {
            return 0;
        }
        filename++;
    }
    return 1;
}

// 验证文件名字符合法性 # 字符验证
uint8_t validate_filename_chars(const char* filename)
{
    if(!filename) {
        return 0;
    }
    
    // 检查非法字符
    const char* illegal_chars = "\\/:*?\"<>|";
    while(*filename) {
        if(strchr(illegal_chars, *filename)) {
            g_unicode_status.error_count++;
            return 0;
        }
        filename++;
    }
    return 1;
}

// 获取转换次数 # 统计查询
uint32_t get_convert_count(void)
{
    return g_unicode_status.convert_count;
}

// 获取错误次数 # 错误统计
uint32_t get_error_count(void)
{
    return g_unicode_status.error_count;
}

// 获取优化次数 # 优化统计
uint32_t get_optimization_count(void)
{
    return g_unicode_status.optimization_count;
}

// 设置优化配置 # 配置设置
void set_optimization_config(const filename_optimization_config_t* config)
{
    if(config && g_unicode_status.initialized) {
        memcpy(&g_optimization_config, config, sizeof(filename_optimization_config_t));
        
        if(g_debug_enabled) {
            my_printf(DEBUG_USART, "optimization config updated\r\n");
        }
    }
}

// 获取优化配置 # 配置查询
filename_optimization_config_t* get_optimization_config(void)
{
    return &g_optimization_config;
}

// 启用调试模式 # 调试控制
void enable_unicode_debug(uint8_t enable)
{
    g_debug_enabled = enable;
    if(g_debug_enabled) {
        my_printf(DEBUG_USART, "unicode debug enabled\r\n");
    }
}

// 显示详细状态信息 # 状态显示
void show_unicode_status(void)
{
    my_printf(DEBUG_USART, "=== Unicode Helper Status ===\r\n");
    my_printf(DEBUG_USART, "Initialized: %s\r\n", g_unicode_status.initialized ? "Yes" : "No");
    my_printf(DEBUG_USART, "Convert Count: %lu\r\n", g_unicode_status.convert_count);
    my_printf(DEBUG_USART, "Error Count: %lu\r\n", g_unicode_status.error_count);
    my_printf(DEBUG_USART, "Optimization Count: %lu\r\n", g_unicode_status.optimization_count);
    my_printf(DEBUG_USART, "Last Error: %d\r\n", g_unicode_status.last_error);
    my_printf(DEBUG_USART, "Max Filename Length: %d\r\n", MAX_FILENAME_LEN);
    my_printf(DEBUG_USART, "Target Length: %d\r\n", g_optimization_config.target_length);
    my_printf(DEBUG_USART, "Debug Mode: %s\r\n", g_debug_enabled ? "On" : "Off");
    my_printf(DEBUG_USART, "============================\r\n");
}

// 记录Unicode操作日志 # 操作日志
void log_unicode_operation(const char* operation, const char* details)
{
    if(g_debug_enabled && operation) {
        if(details) {
            my_printf(DEBUG_USART, "Unicode: %s - %s\r\n", operation, details);
        } else {
            my_printf(DEBUG_USART, "Unicode: %s\r\n", operation);
        }
    }
}

// BCD码转十进制辅助函数 # 本地BCD转换
static uint8_t unicode_bcd_to_dec(uint8_t bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

// 生成短时间戳字符串 # 紧凑时间格式
void generate_short_datetime_string(char* datetime_str)
{
    if(!datetime_str || !g_unicode_status.initialized) {
        return;
    }

    // 引用外部RTC参数
    extern rtc_parameter_struct rtc_initpara;

    // 获取当前RTC时间
    rtc_current_time_get(&rtc_initpara);

    // 转换BCD到十进制
    uint8_t year = unicode_bcd_to_dec(rtc_initpara.year);
    uint8_t month = unicode_bcd_to_dec(rtc_initpara.month);
    uint8_t day = unicode_bcd_to_dec(rtc_initpara.date);
    uint8_t hour = unicode_bcd_to_dec(rtc_initpara.hour);
    uint8_t minute = unicode_bcd_to_dec(rtc_initpara.minute);

    // 格式化为10位数字字符串：YYMMDDHHMM
    sprintf(datetime_str, "%02d%02d%02d%02d%02d", year, month, day, hour, minute);

    if(g_debug_enabled) {
        log_unicode_operation("generate_short_datetime", datetime_str);
    }
}

// 分解文件名组件 # 文件名解析
void get_filename_components(const char* filename, char* path, char* name, char* ext)
{
    if(!filename) {
        return;
    }

    // 初始化输出缓冲区
    if(path) path[0] = '\0';
    if(name) name[0] = '\0';
    if(ext) ext[0] = '\0';

    // 查找最后一个路径分隔符
    const char* last_slash = strrchr(filename, '/');
    if(!last_slash) {
        last_slash = strrchr(filename, '\\');
    }

    // 提取路径部分
    if(path && last_slash) {
        size_t path_len = last_slash - filename;
        strncpy(path, filename, path_len);
        path[path_len] = '\0';
    }

    // 获取文件名部分（不含路径）
    const char* filename_part = last_slash ? (last_slash + 1) : filename;

    // 查找扩展名
    const char* last_dot = strrchr(filename_part, '.');

    // 提取扩展名
    if(ext && last_dot) {
        strcpy(ext, last_dot);
    }

    // 提取文件名（不含扩展名）
    if(name) {
        if(last_dot) {
            size_t name_len = last_dot - filename_part;
            strncpy(name, filename_part, name_len);
            name[name_len] = '\0';
        } else {
            strcpy(name, filename_part);
        }
    }

    if(g_debug_enabled) {
        log_unicode_operation("get_filename_components", filename);
    }
}

// 优化文件名长度 # 文件名优化主函数
void optimize_filename(char* optimized, const char* original, uint16_t max_len)
{
    if(!optimized || !original || !g_unicode_status.initialized) {
        return;
    }

    // 如果原文件名长度在限制内，直接复制
    uint16_t original_len = strlen(original);
    if(original_len <= max_len) {
        strcpy(optimized, original);
        return;
    }

    g_unicode_status.optimization_count++;

    if(g_debug_enabled) {
        char debug_msg[128];
        sprintf(debug_msg, "optimizing: %s (len: %d -> target: %d)", original, original_len, max_len);
        log_unicode_operation("optimize_filename", debug_msg);
    }

    // 分解文件名组件
    char path[64] = {0};
    char name[64] = {0};
    char ext[16] = {0};

    get_filename_components(original, path, name, ext);

    // 优化策略1：前缀优化
    if(g_optimization_config.enable_prefix_optimization) {
        for(int i = 0; g_prefix_mappings[i].original != NULL; i++) {
            if(strncmp(name, g_prefix_mappings[i].original, strlen(g_prefix_mappings[i].original)) == 0) {
                // 找到匹配的前缀，进行替换
                char optimized_name[64];
                sprintf(optimized_name, "%s%s", g_prefix_mappings[i].optimized,
                       name + strlen(g_prefix_mappings[i].original));
                strcpy(name, optimized_name);
                break;
            }
        }
    }

    // 优化策略2：时间戳优化
    if(g_optimization_config.enable_timestamp_optimization && strlen(name) > 14) {
        // 检查是否包含14位时间戳（YYYYMMDDHHMMSS格式）
        char* timestamp_pos = name + strlen(name) - 14;
        uint8_t is_timestamp = 1;

        for(int i = 0; i < 14; i++) {
            if(timestamp_pos[i] < '0' || timestamp_pos[i] > '9') {
                is_timestamp = 0;
                break;
            }
        }

        if(is_timestamp) {
            // 替换为短时间戳
            char short_timestamp[SHORT_DATETIME_LEN];
            generate_short_datetime_string(short_timestamp);

            // 重新组合文件名
            timestamp_pos[0] = '\0';  // 截断原时间戳
            strcat(name, short_timestamp);
        }
    }

    // 优化策略3：多级优化（如果仍然过长）
    if(g_optimization_config.max_optimization_level > 1) {
        // 重新计算当前长度
        char temp_filename[MAX_FILENAME_LEN];
        if(strlen(path) > 0) {
            sprintf(temp_filename, "%s/%s%s", path, name, ext);
        } else {
            sprintf(temp_filename, "%s%s", name, ext);
        }

        // 如果仍然过长，进行进一步优化
        if(strlen(temp_filename) > max_len && g_optimization_config.max_optimization_level >= 2) {
            // 策略3a：截断文件名主体部分
            uint16_t available_len = max_len - strlen(path) - strlen(ext) - 1; // 减去路径、扩展名和分隔符
            if(available_len > 10 && strlen(name) > available_len) {
                // 保留前缀和时间戳，截断中间部分
                if(strlen(name) > 20) {
                    char truncated_name[64];
                    strncpy(truncated_name, name, available_len - 3);
                    truncated_name[available_len - 3] = '\0';
                    strcat(truncated_name, "...");
                    strcpy(name, truncated_name);
                }
            }
        }

        // 如果还是过长，进行最激进的优化
        if(strlen(temp_filename) > max_len && g_optimization_config.max_optimization_level >= 3) {
            // 策略3b：使用哈希值缩短文件名
            uint32_t hash = 0;
            const char* p = name;
            while(*p) {
                hash = hash * 31 + *p;
                p++;
            }
            sprintf(name, "opt_%08lx", hash);
        }
    }

    // 重新组合完整文件名
    if(strlen(path) > 0) {
        sprintf(optimized, "%s/%s%s", path, name, ext);
    } else {
        sprintf(optimized, "%s%s", name, ext);
    }

    if(g_debug_enabled) {
        char debug_msg[128];
        sprintf(debug_msg, "optimized to: %s (len: %d)", optimized, strlen(optimized));
        log_unicode_operation("optimize_result", debug_msg);
    }
}

// 清理文件名非法字符 # 字符清理
void sanitize_filename(char* filename)
{
    if(!filename) {
        return;
    }

    // 非法字符替换表
    const char* illegal_chars = "\\/:*?\"<>|";
    const char replacement = '_';

    char* p = filename;
    uint8_t modified = 0;

    while(*p) {
        if(strchr(illegal_chars, *p)) {
            *p = replacement;
            modified = 1;
        }
        p++;
    }

    if(modified) {
        g_unicode_status.error_count++;
        if(g_debug_enabled) {
            log_unicode_operation("sanitize_filename", filename);
        }
    }
}



#endif // UNICODE_SUPPORT_ENABLED
