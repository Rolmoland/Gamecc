#include "mcu_cmic_gd32f470vet6.h"
#include "fatfs_unicode_helper.h"
#include "diskio.h"
#include <stdarg.h>

// 静态变量定义 # 模块内部状态管理
static FIL g_sample_file;                   // 文件句柄 # FATFS文件对象
static uint8_t g_file_opened = 0;           // 文件打开状态 # 0:关闭 1:打开
static uint8_t g_record_count = 0;          // 当前文件记录计数 # 文件内记录数

// 全局变量定义 # 对外可见状态
uint8_t g_sample_record_count = 0;          // 当前文件记录计数 # 外部可访问的计数器
uint8_t g_overlimit_record_count = 0;       // 超限文件记录计数 # 外部可访问的超限计数器
uint8_t g_hidedata_record_count = 0;        // 隐藏文件记录计数 # 外部可访问的隐藏计数器
FATFS g_sample_fs;                          // 文件系统对象 # 数据存储专用文件系统（全局可访问）

// 超限数据存储静态变量 # 超限数据模块状态管理
static FIL g_overlimit_file;                // 超限文件句柄 # FATFS超限文件对象
static uint8_t g_overlimit_file_opened = 0; // 超限文件打开状态 # 0:关闭 1:打开
static uint8_t g_overlimit_record_count_internal = 0; // 超限文件内部记录计数 # 文件内记录数

// 日志系统静态变量 # 日志系统状态管理
static FIL g_log_file;                       // 日志文件句柄 # FATFS日志文件对象
static uint8_t g_log_file_opened = 0;        // 日志文件打开状态 # 0:关闭 1:打开
static uint32_t g_boot_count = 0;            // 上电次数计数器 # 系统启动次数记录

// 隐藏数据存储静态变量 # 隐藏数据模块状态管理
static FIL g_hidedata_file;                 // 隐藏文件句柄 # FATFS隐藏文件对象
static uint8_t g_hidedata_file_opened = 0;  // 隐藏文件打开状态 # 0:关闭 1:打开
static uint8_t g_hidedata_record_count_internal = 0; // 隐藏文件内部记录计数 # 文件内记录数

// SD卡和文件系统状态管理 # 统一的存储状态管理
static uint8_t g_sd_initialized = 0;         // SD卡初始化状态 # 0:未初始化 1:已初始化
static uint8_t g_fs_mounted = 0;             // 文件系统挂载状态 # 0:未挂载 1:已挂载

// 统一的SD卡初始化和文件系统挂载 # SD卡管理函数
static uint8_t ensure_sd_ready(void)
{
    DSTATUS disk_status;                     // 磁盘状态 # 磁盘初始化状态
    FRESULT res;                             // FATFS操作结果 # 文件系统返回值

    // 如果SD卡已经初始化且文件系统已挂载，直接返回成功
    if(g_sd_initialized && g_fs_mounted) {
        return 1;                            // 已就绪
    }

    // 如果SD卡未初始化，尝试初始化
    if(!g_sd_initialized) {
        disk_status = disk_initialize(0);
        if(disk_status != RES_OK) {
            my_printf(DEBUG_USART, "SD card initialization failed, status: %d\r\n", disk_status);
            return 0;                        // SD卡初始化失败
        } else {
            g_sd_initialized = 1;            // 标记SD卡已初始化
        }
    }

    // 如果文件系统未挂载，尝试挂载
    if(!g_fs_mounted) {
        res = f_mount(0, &g_sample_fs);
        if(res != FR_OK) {
            my_printf(DEBUG_USART, "File system mount failed, error: %d\r\n", res);
            return 0;                        // 挂载失败
        } else {
            g_fs_mounted = 1;                // 标记文件系统已挂载
        }
    }

    return 1;                                // SD卡和文件系统就绪
}

// 从Flash读取上电次数 # Flash读取函数
static uint32_t read_boot_count_from_flash(void)
{
    uint8_t read_buffer[8] = {0};            // 读取缓冲区 # Flash读取数据缓冲区
    uint32_t boot_count = 0;                 // 上电次数 # 默认值为0

    // 从Flash读取数据
    spi_flash_buffer_read(read_buffer, LOG_BOOT_COUNT_FLASH_ADDR, 8);

    // 检查标识符"BOOT"
    if(strncmp((char*)read_buffer, "BOOT", 4) == 0) {
        // 解析上电次数
        boot_count |= (uint32_t)read_buffer[4] << 0;
        boot_count |= (uint32_t)read_buffer[5] << 8;
        boot_count |= (uint32_t)read_buffer[6] << 16;
        boot_count |= (uint32_t)read_buffer[7] << 24;
    } else {
    }

    return boot_count;
}

// 保存上电次数到Flash # Flash写入函数
static void save_boot_count_to_flash(uint32_t boot_count)
{
    uint8_t write_buffer[8] = {0};           // 写入缓冲区 # Flash写入数据缓冲区

    // 构建保存数据
    strcpy((char*)write_buffer, "BOOT");     // 标识符
    write_buffer[4] = (boot_count >> 0) & 0xFF;
    write_buffer[5] = (boot_count >> 8) & 0xFF;
    write_buffer[6] = (boot_count >> 16) & 0xFF;
    write_buffer[7] = (boot_count >> 24) & 0xFF;

    // 擦除扇区
    spi_flash_sector_erase(LOG_BOOT_COUNT_FLASH_ADDR);

    // 等待擦除完成
    spi_flash_wait_for_write_end();

    // 写入数据
    spi_flash_buffer_write(write_buffer, LOG_BOOT_COUNT_FLASH_ADDR, 8);
}

// sample目录已预先创建，无需创建函数 # 目录已存在

// overLimit目录已预先创建，无需创建函数 # 目录已存在

// log目录已预先创建，无需创建函数 # 目录已存在

// hideData目录已预先创建，无需创建函数 # 目录已存在

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

    // 确保SD卡就绪
    if(!ensure_sd_ready()) {
        my_printf(DEBUG_USART, "SD card not ready for sample file creation\r\n");
        return 0;  // SD卡未就绪
    }

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
        return 1;                            // 创建成功
    }
    else {
        my_printf(DEBUG_USART, "Failed to create sample file: %s, error: %d\r\n", optimized_filename, res);
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

    // 确保SD卡就绪
    if(!ensure_sd_ready()) {
        return 0;  // SD卡未就绪
    }

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
        return 1;                            // 创建成功
    }
    else {
        return 0;                            // 创建失败
    }
}

// 创建新的日志文件 # 日志文件创建函数
static uint8_t create_new_log_file(uint32_t boot_id)
{
    char filename[LOG_FILENAME_MAX_LEN];     // 文件名缓冲区 # 日志文件名
    FRESULT res;                             // FATFS操作结果 # 文件系统返回值

    // 确保SD卡就绪
    if(!ensure_sd_ready()) {
        return 0;  // SD卡未就绪
    }

    // 构建日志文件名：log目录/log{id}.txt
    sprintf(filename, "%s/log%lu.txt", LOG_DIR_NAME, boot_id);

    // 创建并打开文件（总是创建新文件，覆盖同名文件）
    res = f_open(&g_log_file, filename, FA_CREATE_ALWAYS | FA_WRITE);

    // 检查文件创建结果
    if(res == FR_OK) {

        // 立即同步到存储设备
        f_sync(&g_log_file);

        return 1;                            // 创建成功
    }
    else {
        my_printf(DEBUG_USART, "Failed to create log file: %s, error: %d\r\n", filename, res);
        return 0;                            // 创建失败
    }
}

// 创建新的隐藏数据文件 # 隐藏文件创建函数
static uint8_t create_new_hidedata_file(void)
{
    char timestamp[15];                      // 时间戳缓冲区 # 14位时间戳+结束符
    char filename[HIDEDATA_FILENAME_MAX_LEN]; // 文件名缓冲区 # 原始文件名
    FRESULT res;                             // FATFS操作结果 # 文件系统返回值

    // 确保SD卡就绪
    if(!ensure_sd_ready()) {
        return 0;  // SD卡未就绪
    }

    // 生成14位时间戳
    generate_14digit_timestamp(timestamp);

    // 构建完整文件名：hideData目录/hideData时间戳.txt
    sprintf(filename, "%s/hideData%s.txt", HIDEDATA_DIR_NAME, timestamp);

    // 创建并打开文件（总是创建新文件，覆盖同名文件）
    res = f_open(&g_hidedata_file, filename, FA_CREATE_ALWAYS | FA_WRITE);

    // 检查文件创建结果
    if(res == FR_OK) {
        // 立即同步到存储设备
        f_sync(&g_hidedata_file);

        return 1;                            // 创建成功
    }
    else {
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

// 格式化隐藏数据 # 隐藏数据格式化函数
static void format_hidedata(char* buffer, float voltage, uint8_t is_overlimit)
{
    extern rtc_parameter_struct rtc_initpara; // RTC时间参数 # 引用外部RTC参数
    char hide_data[32];                      // 隐藏数据缓冲区 # 加密数据存储

    // 获取当前RTC时间
    rtc_current_time_get(&rtc_initpara);

    // 调用hide_conversion函数进行数据加密
    sprintf(hide_data, "%.2fV", voltage);
    hide_conversion((uint8_t*)hide_data);

    // 如果是超限数据，在加密后的内容后加*
    if(is_overlimit) {
        strcat(hide_data, "*");
    }

    // 格式化数据为"YYYY-MM-DD HH:MM:SS 加密数据"格式
    sprintf(buffer, "20%02d-%02d-%02d %02d:%02d:%02d %s\r\n",
            bcd_to_dec(rtc_initpara.year),
            bcd_to_dec(rtc_initpara.month),
            bcd_to_dec(rtc_initpara.date),
            bcd_to_dec(rtc_initpara.hour),
            bcd_to_dec(rtc_initpara.minute),
            bcd_to_dec(rtc_initpara.second),
            hide_data);
}

// 格式化超限数据 # 超限数据格式化函数
static void format_overlimit_data(char* buffer, float voltage, float limit_value)
{
    extern rtc_parameter_struct rtc_initpara; // RTC时间参数 # 引用外部RTC参数

    // 获取当前RTC时间
    rtc_current_time_get(&rtc_initpara);

    // 格式化数据为"YYYY-MM-DD HH:MM:SS XX.XXV limit XX.XXV"格式
    sprintf(buffer, "20%02d-%02d-%02d %02d:%02d:%02d %.2fV limit %.2fV\r\n",
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
        return 0;                            // 写入失败
    }
}

// 保存采样数据到TF卡 # 主接口函数
void save_sample_data(float voltage)
{
    char data_buffer[SAMPLE_DATA_BUFFER_SIZE]; // 数据缓冲区 # 格式化数据存储

    // 检查是否需要创建新文件（记录数达到限制或文件未打开）
    if(g_record_count >= SAMPLE_RECORDS_PER_FILE || !g_file_opened) {
        // 如果当前有文件打开，先关闭
        if(g_file_opened) {
            f_close(&g_sample_file);
            g_file_opened = 0;               // 更新文件状态
        }

        // 创建新文件
        if(create_new_sample_file()) {
            g_file_opened = 1;               // 标记文件已打开
            g_record_count = 0;              // 重置记录计数
        } else {
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
        }
    }
}

// 写入隐藏数据到文件 # 隐藏数据写入函数
static uint8_t write_hidedata(const char* data)
{
    UINT bw;                                 // 实际写入字节数 # FATFS写入计数
    FRESULT res;                             // FATFS操作结果 # 文件系统返回值

    // 写入数据到文件，参考sd_app.c的f_write操作模式
    res = f_write(&g_hidedata_file, data, strlen(data), &bw);

    // 检查写入结果
    if(res == FR_OK && bw == strlen(data)) {
        // 立即同步到存储设备，确保数据持久化
        f_sync(&g_hidedata_file);
        return 1;                            // 写入成功
    }
    else {
        return 0;                            // 写入失败
    }
}

// 写入日志数据到文件 # 日志写入函数
static uint8_t write_log_data(const char* data)
{
    UINT bw;                                 // 实际写入字节数 # FATFS写入计数
    FRESULT res;                             // FATFS操作结果 # 文件系统返回值

    if(!g_log_file_opened) {
        return 0;                            // 日志文件未打开
    }

    // 写入数据到文件
    res = f_write(&g_log_file, data, strlen(data), &bw);

    // 检查写入结果
    if(res == FR_OK && bw == strlen(data)) {
        // 立即同步到存储设备，确保数据持久化
        f_sync(&g_log_file);
        return 1;                            // 写入成功
    }
    else {
        return 0;                            // 写入失败
    }
}

// 日志系统初始化 # 日志系统初始化函数
void log_init(void)
{
    // 读取上电次数
    g_boot_count = read_boot_count_from_flash();

    // 确保SD卡就绪
    if(!ensure_sd_ready()) {
        my_printf(DEBUG_USART, "SD card not ready for log file creation\r\n");
        return;  // SD卡未就绪，无法创建文件
    }

    // 创建日志文件（使用当前的boot_count作为文件号）
    if(create_new_log_file(g_boot_count)) {
        g_log_file_opened = 1;               // 标记日志文件已打开

        // 记录RTC配置日志
        log_write("rtc config");

        // 递增上电次数并保存到Flash（为下次上电准备）
        g_boot_count++;
        save_boot_count_to_flash(g_boot_count);
    }
}

// 写入日志消息 # 日志记录接口函数
void log_write(const char* message)
{
    if(!message) {
        return;                              // 参数无效
    }

    // 如果日志文件未打开，尝试重新初始化
    if(!g_log_file_opened) {
        // 使用当前boot_count减1，因为在log_init中已经递增了
        uint32_t current_file_id = (g_boot_count > 0) ? (g_boot_count - 1) : 0;

        // 尝试创建日志文件
        if(create_new_log_file(current_file_id)) {
            g_log_file_opened = 1;           // 标记日志文件已打开
        } else {
            return;                          // 重新初始化失败，放弃写入
        }
    }

    char log_buffer[LOG_DATA_BUFFER_SIZE];   // 日志缓冲区 # 格式化日志数据
    extern rtc_parameter_struct rtc_initpara; // RTC时间参数 # 引用外部RTC参数

    // 获取当前RTC时间
    rtc_current_time_get(&rtc_initpara);

    // 格式化日志数据：时间戳 + 消息（格式：2025-01-01 10:00:01 message）
    sprintf(log_buffer, "20%02d-%02d-%02d %02d:%02d:%02d %s\r\n",
            bcd_to_dec(rtc_initpara.year),
            bcd_to_dec(rtc_initpara.month),
            bcd_to_dec(rtc_initpara.date),
            bcd_to_dec(rtc_initpara.hour),
            bcd_to_dec(rtc_initpara.minute),
            bcd_to_dec(rtc_initpara.second),
            message);

    // 写入日志
    write_log_data(log_buffer);
}

// 格式化日志输出 # 格式化日志接口函数
void log_printf(const char* format, ...)
{
    if(!format || !g_log_file_opened) {
        return;                              // 参数无效或文件未打开
    }

    char message_buffer[LOG_DATA_BUFFER_SIZE - 32]; // 消息缓冲区 # 预留时间戳空间
    va_list args;                            // 可变参数列表 # 格式化参数

    // 格式化消息
    va_start(args, format);
    vsnprintf(message_buffer, sizeof(message_buffer), format, args);
    va_end(args);

    // 调用log_write写入日志
    log_write(message_buffer);
}

// 保存超限数据到TF卡 # 超限数据主接口函数
void save_overlimit_data(float voltage, float limit_value)
{
    char data_buffer[OVERLIMIT_DATA_BUFFER_SIZE]; // 数据缓冲区 # 格式化数据存储

    // 检查是否需要创建新文件（记录数达到限制或文件未打开）
    if(g_overlimit_record_count_internal >= OVERLIMIT_RECORDS_PER_FILE || !g_overlimit_file_opened) {
        // 如果当前有文件打开，先关闭
        if(g_overlimit_file_opened) {
            f_close(&g_overlimit_file);
            g_overlimit_file_opened = 0;    // 更新文件状态
        }

        // 创建新文件
        if(create_new_overlimit_file()) {
            g_overlimit_file_opened = 1;     // 标记文件已打开
            g_overlimit_record_count_internal = 0; // 重置记录计数
        } else {
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
        }
    }
}

// 保存隐藏数据到TF卡 # 隐藏数据主接口函数
void save_hidedata(float voltage)
{
    char data_buffer[HIDEDATA_DATA_BUFFER_SIZE]; // 数据缓冲区 # 格式化数据存储
    extern float g_limit_value;              // 引入限制值 # 用于判断是否超限
    uint8_t is_overlimit = (voltage > g_limit_value); // 是否超限 # 超限判断

    // 检查是否需要创建新文件（记录数达到限制或文件未打开）
    if(g_hidedata_record_count_internal >= HIDEDATA_RECORDS_PER_FILE || !g_hidedata_file_opened) {
        // 如果当前有文件打开，先关闭
        if(g_hidedata_file_opened) {
            f_close(&g_hidedata_file);
            g_hidedata_file_opened = 0;     // 更新文件状态
        }

        // 创建新文件
        if(create_new_hidedata_file()) {
            g_hidedata_file_opened = 1;     // 标记文件已打开
            g_hidedata_record_count_internal = 0; // 重置记录计数
        } else {
            return;                          // 文件创建失败，退出
        }
    }

    // 如果文件已打开，写入数据
    if(g_hidedata_file_opened) {
        // 格式化数据
        format_hidedata(data_buffer, voltage, is_overlimit);

        // 写入数据到文件
        if(write_hidedata(data_buffer)) {
            g_hidedata_record_count_internal++; // 增加记录计数
            g_hidedata_record_count = g_hidedata_record_count_internal; // 更新全局计数器
        }
    }
}

