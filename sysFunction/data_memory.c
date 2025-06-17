#include "mcu_cmic_gd32f470vet6.h"
#include "fatfs_unicode_helper.h"
#include "diskio.h"

// 静态变量定义 # 模块内部状态管理
static FIL g_sample_file;                   // 文件句柄 # FATFS文件对象
static uint8_t g_file_opened = 0;           // 文件打开状态 # 0:关闭 1:打开
static uint8_t g_record_count = 0;          // 当前文件记录计数 # 文件内记录数

// 全局变量定义 # 对外可见状态
uint8_t g_sample_record_count = 0;          // 当前文件记录计数 # 外部可访问的计数器
uint8_t g_overlimit_record_count = 0;       // 超限文件记录计数 # 外部可访问的超限计数器
static FATFS g_sample_fs;                   // 文件系统对象 # 数据存储专用文件系统

// 超限数据存储静态变量 # 超限数据模块状态管理
static FIL g_overlimit_file;                // 超限文件句柄 # FATFS超限文件对象
static uint8_t g_overlimit_file_opened = 0; // 超限文件打开状态 # 0:关闭 1:打开
static uint8_t g_overlimit_record_count_internal = 0; // 超限文件内部记录计数 # 文件内记录数

// 创建sample目录 # 目录创建函数
static uint8_t create_sample_directory(void)
{
    FRESULT res;                             // FATFS操作结果 # 文件系统返回值
    DSTATUS disk_status;                     // 磁盘状态 # 磁盘初始化状态

    // 首先初始化磁盘（SD卡）
    disk_status = disk_initialize(0);
    if(disk_status != RES_OK) {
        my_printf(DEBUG_USART, "Failed to initialize SD card, status: %d\r\n", disk_status);
        return 0;                            // SD卡初始化失败
    }

    // 然后挂载文件系统
    res = f_mount(0, &g_sample_fs);
    if(res != FR_OK) {
        my_printf(DEBUG_USART, "Failed to mount filesystem for sample directory, error: %d\r\n", res);
        return 0;                            // 挂载失败
    }

    // 尝试创建目录
    res = f_mkdir(SAMPLE_DIR_NAME);

    // 检查创建结果
    if(res == FR_OK) {
        my_printf(DEBUG_USART, "Sample directory created successfully\r\n");
        return 1;                            // 创建成功
    }
    else if(res == FR_EXIST) {
        my_printf(DEBUG_USART, "Sample directory already exists\r\n");
        return 1;                            // 目录已存在，视为成功
    }
    else {
        my_printf(DEBUG_USART, "Failed to create sample directory, error: %d\r\n", res);
        return 0;                            // 创建失败
    }
}

// 创建overLimit目录 # 超限目录创建函数
static uint8_t create_overlimit_directory(void)
{
    FRESULT res;                             // FATFS操作结果 # 文件系统返回值
    DSTATUS disk_status;                     // 磁盘状态 # 磁盘初始化状态

    // 首先初始化磁盘（SD卡）
    disk_status = disk_initialize(0);
    if(disk_status != RES_OK) {
        my_printf(DEBUG_USART, "Failed to initialize SD card for overlimit, status: %d\r\n", disk_status);
        return 0;                            // SD卡初始化失败
    }

    // 然后挂载文件系统
    res = f_mount(0, &g_sample_fs);
    if(res != FR_OK) {
        my_printf(DEBUG_USART, "Failed to mount filesystem for overlimit directory, error: %d\r\n", res);
        return 0;                            // 挂载失败
    }

    // 尝试创建目录
    res = f_mkdir(OVERLIMIT_DIR_NAME);

    // 检查创建结果
    if(res == FR_OK) {
        my_printf(DEBUG_USART, "Overlimit directory created successfully\r\n");
        return 1;                            // 创建成功
    }
    else if(res == FR_EXIST) {
        my_printf(DEBUG_USART, "Overlimit directory already exists\r\n");
        return 1;                            // 目录已存在，视为成功
    }
    else {
        my_printf(DEBUG_USART, "Failed to create overlimit directory, error: %d\r\n", res);
        return 0;                            // 创建失败
    }
}

// BCD码转十进制辅助函数 # BCD转换工具
static uint8_t bcd_to_dec(uint8_t bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

// 生成14位时间戳字符串 # 文件名时间戳生成
static void generate_14digit_timestamp(char* timestamp_str)
{
    extern rtc_parameter_struct rtc_initpara; // RTC时间参数 # 引用外部RTC参数

    // 获取当前RTC时间
    rtc_current_time_get(&rtc_initpara);

    // 转换BCD到十进制并格式化为14位时间戳：YYYYMMDDHHMMSS
    sprintf(timestamp_str, "20%02d%02d%02d%02d%02d%02d",
            bcd_to_dec(rtc_initpara.year),
            bcd_to_dec(rtc_initpara.month),
            bcd_to_dec(rtc_initpara.date),
            bcd_to_dec(rtc_initpara.hour),
            bcd_to_dec(rtc_initpara.minute),
            bcd_to_dec(rtc_initpara.second));
}

// 创建新的采样数据文件 # 文件创建函数
static uint8_t create_new_sample_file(void)
{
    char timestamp[15];                      // 时间戳缓冲区 # 14位时间戳+结束符
    char filename[SAMPLE_FILENAME_MAX_LEN];  // 文件名缓冲区 # 原始文件名
    char optimized_filename[SAMPLE_FILENAME_MAX_LEN]; // 优化后文件名 # 文件名优化结果
    FRESULT res;                             // FATFS操作结果 # 文件系统返回值

    // 生成14位时间戳
    generate_14digit_timestamp(timestamp);

    // 构建完整文件名：sample目录/data时间戳.txt（避免前缀优化冲突）
    sprintf(filename, "%s/data%s.txt", SAMPLE_DIR_NAME, timestamp);

    // 直接使用文件名，不进行优化（避免路径被错误处理）
    strcpy(optimized_filename, filename);

    // 创建并打开文件（总是创建新文件，覆盖同名文件）
    res = f_open(&g_sample_file, optimized_filename, FA_CREATE_ALWAYS | FA_WRITE);

    // 检查文件创建结果
    if(res == FR_OK) {
        my_printf(DEBUG_USART, "Sample file created: %s\r\n", optimized_filename);
        return 1;                            // 创建成功
    }
    else {
        my_printf(DEBUG_USART, "Failed to create sample file: %s, error: %d\r\n",
                 optimized_filename, res);
        return 0;                            // 创建失败
    }
}

// 创建新的超限数据文件 # 超限文件创建函数
static uint8_t create_new_overlimit_file(void)
{
    char timestamp[15];                      // 时间戳缓冲区 # 14位时间戳+结束符
    char filename[OVERLIMIT_FILENAME_MAX_LEN]; // 文件名缓冲区 # 原始文件名
    char optimized_filename[OVERLIMIT_FILENAME_MAX_LEN]; // 优化后文件名 # 文件名优化结果
    FRESULT res;                             // FATFS操作结果 # 文件系统返回值

    // 生成14位时间戳
    generate_14digit_timestamp(timestamp);

    // 构建完整文件名：overLimit目录/limit时间戳.txt（避免前缀优化冲突）
    sprintf(filename, "%s/limit%s.txt", OVERLIMIT_DIR_NAME, timestamp);

    // 直接使用文件名，不进行优化（避免路径被错误处理）
    strcpy(optimized_filename, filename);

    // 创建并打开文件（总是创建新文件，覆盖同名文件）
    res = f_open(&g_overlimit_file, optimized_filename, FA_CREATE_ALWAYS | FA_WRITE);

    // 检查文件创建结果
    if(res == FR_OK) {
        my_printf(DEBUG_USART, "Overlimit file created: %s\r\n", optimized_filename);
        return 1;                            // 创建成功
    }
    else {
        my_printf(DEBUG_USART, "Failed to create overlimit file: %s, error: %d\r\n",
                 optimized_filename, res);
        return 0;                            // 创建失败
    }
}

// 格式化采样数据 # 数据格式化函数
static void format_sample_data(char* buffer, float voltage)
{
    extern rtc_parameter_struct rtc_initpara; // RTC时间参数 # 引用外部RTC参数

    // 获取当前RTC时间
    rtc_current_time_get(&rtc_initpara);

    // 格式化数据为"YYYY-MM-DD HH:MM:SS X.XXV"格式，参考sampling_control.c的时间格式
    sprintf(buffer, "20%02d-%02d-%02d %02d:%02d:%02d %.2fV\r\n",
            bcd_to_dec(rtc_initpara.year),
            bcd_to_dec(rtc_initpara.month),
            bcd_to_dec(rtc_initpara.date),
            bcd_to_dec(rtc_initpara.hour),
            bcd_to_dec(rtc_initpara.minute),
            bcd_to_dec(rtc_initpara.second),
            voltage);
}

// 格式化超限数据 # 超限数据格式化函数
static void format_overlimit_data(char* buffer, float voltage, float limit_value)
{
    extern rtc_parameter_struct rtc_initpara; // RTC时间参数 # 引用外部RTC参数

    // 获取当前RTC时间
    rtc_current_time_get(&rtc_initpara);

    // 格式化数据为"YYYY-MM-DD HH:MM:SS XXV limit XXV"格式
    sprintf(buffer, "20%02d-%02d-%02d %02d:%02d:%02d %.0fV limit %.0fV\r\n",
            bcd_to_dec(rtc_initpara.year),
            bcd_to_dec(rtc_initpara.month),
            bcd_to_dec(rtc_initpara.date),
            bcd_to_dec(rtc_initpara.hour),
            bcd_to_dec(rtc_initpara.minute),
            bcd_to_dec(rtc_initpara.second),
            voltage, limit_value);
}

// 写入采样数据到文件 # 数据写入函数
static uint8_t write_sample_data(const char* data)
{
    UINT bw;                                 // 实际写入字节数 # FATFS写入计数
    FRESULT res;                             // FATFS操作结果 # 文件系统返回值

    // 写入数据到文件，参考sd_app.c的f_write操作模式
    res = f_write(&g_sample_file, data, strlen(data), &bw);

    // 检查写入结果
    if(res == FR_OK && bw == strlen(data)) {
        // 立即同步到存储设备，确保数据持久化
        f_sync(&g_sample_file);
        return 1;                            // 写入成功
    }
    else {
        my_printf(DEBUG_USART, "Failed to write sample data, error: %d, written: %d\r\n",
                 res, bw);
        return 0;                            // 写入失败
    }
}

// 写入超限数据到文件 # 超限数据写入函数
static uint8_t write_overlimit_data(const char* data)
{
    UINT bw;                                 // 实际写入字节数 # FATFS写入计数
    FRESULT res;                             // FATFS操作结果 # 文件系统返回值

    // 写入数据到文件，参考sd_app.c的f_write操作模式
    res = f_write(&g_overlimit_file, data, strlen(data), &bw);

    // 检查写入结果
    if(res == FR_OK && bw == strlen(data)) {
        // 立即同步到存储设备，确保数据持久化
        f_sync(&g_overlimit_file);
        return 1;                            // 写入成功
    }
    else {
        my_printf(DEBUG_USART, "Failed to write overlimit data, error: %d, written: %d\r\n",
                 res, bw);
        return 0;                            // 写入失败
    }
}

// 保存采样数据到TF卡 # 主接口函数
void save_sample_data(float voltage)
{
    static uint8_t dir_created = 0;         // 目录创建标志 # 静态标志避免重复创建
    char data_buffer[SAMPLE_DATA_BUFFER_SIZE]; // 数据缓冲区 # 格式化数据存储

    // 首次调用时创建sample目录
    if(!dir_created) {
        if(create_sample_directory()) {
            dir_created = 1;                 // 标记目录已创建
        } else {
            my_printf(DEBUG_USART, "Failed to create sample directory, data not saved\r\n");
            return;                          // 目录创建失败，退出
        }
    }

    // 检查是否需要创建新文件（记录数达到限制或文件未打开）
    if(g_record_count >= SAMPLE_RECORDS_PER_FILE || !g_file_opened) {
        // 如果当前有文件打开，先关闭
        if(g_file_opened) {
            f_close(&g_sample_file);
            my_printf(DEBUG_USART, "Sample file closed, %d records saved\r\n", g_record_count);
            g_file_opened = 0;               // 更新文件状态
        }

        // 创建新文件
        if(create_new_sample_file()) {
            g_file_opened = 1;               // 标记文件已打开
            g_record_count = 0;              // 重置记录计数
        } else {
            my_printf(DEBUG_USART, "Failed to create new sample file, data not saved\r\n");
            return;                          // 文件创建失败，退出
        }
    }

    // 如果文件已打开，写入数据
    if(g_file_opened) {
        // 格式化数据
        format_sample_data(data_buffer, voltage);

        // 写入数据到文件
        if(write_sample_data(data_buffer)) {
            g_record_count++;                // 增加记录计数
            g_sample_record_count = g_record_count; // 更新全局计数器
        } else {
            my_printf(DEBUG_USART, "Failed to write sample data\r\n");
        }
    }
}

// 保存超限数据到TF卡 # 超限数据主接口函数
void save_overlimit_data(float voltage, float limit_value)
{
    static uint8_t overlimit_dir_created = 0; // 超限目录创建标志 # 静态标志避免重复创建
    char data_buffer[OVERLIMIT_DATA_BUFFER_SIZE]; // 数据缓冲区 # 格式化数据存储

    // 首次调用时创建overLimit目录
    if(!overlimit_dir_created) {
        if(create_overlimit_directory()) {
            overlimit_dir_created = 1;       // 标记目录已创建
        } else {
            my_printf(DEBUG_USART, "Failed to create overlimit directory, data not saved\r\n");
            return;                          // 目录创建失败，退出
        }
    }

    // 检查是否需要创建新文件（记录数达到限制或文件未打开）
    if(g_overlimit_record_count_internal >= OVERLIMIT_RECORDS_PER_FILE || !g_overlimit_file_opened) {
        // 如果当前有文件打开，先关闭
        if(g_overlimit_file_opened) {
            f_close(&g_overlimit_file);
            my_printf(DEBUG_USART, "Overlimit file closed, %d records saved\r\n", g_overlimit_record_count_internal);
            g_overlimit_file_opened = 0;    // 更新文件状态
        }

        // 创建新文件
        if(create_new_overlimit_file()) {
            g_overlimit_file_opened = 1;     // 标记文件已打开
            g_overlimit_record_count_internal = 0; // 重置记录计数
        } else {
            my_printf(DEBUG_USART, "Failed to create new overlimit file, data not saved\r\n");
            return;                          // 文件创建失败，退出
        }
    }

    // 如果文件已打开，写入数据
    if(g_overlimit_file_opened) {
        // 格式化数据
        format_overlimit_data(data_buffer, voltage, limit_value);

        // 写入数据到文件
        if(write_overlimit_data(data_buffer)) {
            g_overlimit_record_count_internal++; // 增加记录计数
            g_overlimit_record_count = g_overlimit_record_count_internal; // 更新全局计数器
        } else {
            my_printf(DEBUG_USART, "Failed to write overlimit data\r\n");
        }
    }
}

