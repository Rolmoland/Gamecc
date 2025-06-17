#include "mcu_cmic_gd32f470vet6.h"

// 全局文件系统对象
static FATFS g_fs;          // 文件系统对象

extern __IO uint8_t rx_flag;
extern uint8_t uart_dma_buffer[512];

// 全局配置变量
float g_ratio_value = 1.0f;  // 全局Ratio值
float g_limit_value = 1.0f;  // 全局Limit值

// 检查字符串是否为有效浮点数
static uint8_t is_valid_float(const char *str)
{
    uint8_t has_digit = 0;
    uint8_t has_dot = 0;
    
    // 跳过前导空格
    while(*str == ' ' || *str == '\t') str++;
    
    // 检查符号
    if(*str == '+' || *str == '-') str++;
    
    // 检查数字和小数点
    while(*str) {
        if(*str >= '0' && *str <= '9') {
            has_digit = 1;
        } else if(*str == '.' && !has_dot) {
            has_dot = 1;
        } else {
            return 0; // 非法字符
        }
        str++;
    }
    
    return has_digit; // 至少要有一个数字
}

// 自定义字符串转浮点数函数
static float str_to_float(const char* str)
{
    float result = 0.0f;
    float sign = 1.0f;
    int i = 0;
    
    // 处理符号
    if(str[i] == '-') {
        sign = -1.0f;
        i++;
    } else if(str[i] == '+') {
        i++;
    }
    
    // 处理整数部分
    while(str[i] >= '0' && str[i] <= '9') {
        result = result * 10.0f + (str[i] - '0');
        i++;
    }
    
    // 处理小数部分
    if(str[i] == '.') {
        i++;
        float decimal = 0.1f;
        while(str[i] >= '0' && str[i] <= '9') {
            result += (str[i] - '0') * decimal;
            decimal *= 0.1f;
            i++;
        }
    }
    
    return result * sign;
}

void config_read(uint8_t * cmd)
{
    // 检查是否为conf命令（确保是精确匹配）
    if(strncmp((char*)cmd, "conf", 4) == 0 && (cmd[4] == '\0' || cmd[4] == '\r' || cmd[4] == '\n' || cmd[4] == ' ')) 
    {
        FIL file;           // 文件对象
        FRESULT fr;         // FatFs返回值
        char buffer[256];   // 读取缓冲区
        char line[256];     // 行缓冲区
        char ratio_value[32] = {0};  // 存储Ratio值
        char limit_value[32] = {0};  // 存储Limit值
        uint8_t ratio_found = 0;     // 是否找到Ratio
        uint8_t limit_found = 0;     // 是否找到Limit
        UINT br;            // 读取的字节数
        
        // 挂载文件系统
        fr = f_mount(0, &g_fs);
        if(fr != FR_OK) {
            my_printf(DEBUG_USART, "config.ini file not found.\r\n");
            return;
        }
        
        // 打开config.ini文件
        fr = f_open(&file, "0:config.ini", FA_READ);
        if(fr != FR_OK) {
            my_printf(DEBUG_USART, "config.ini file not found.\r\n");
            f_mount(0, NULL); // 卸载文件系统
            return;
        }
        
        // 读取文件内容
        fr = f_read(&file, buffer, sizeof(buffer) - 1, &br);
        if(fr != FR_OK || br == 0) {
            my_printf(DEBUG_USART, "config.ini file read error.\r\n");
            f_close(&file);
            f_mount(0, NULL);
            return;
        }
        
        buffer[br] = '\0'; // 确保字符串结束
        
        // 解析文件内容
        uint8_t in_ratio_section = 0;
        uint8_t in_limit_section = 0;
        char *ptr = buffer;
        int i = 0;
        
        // 逐行解析
        while(*ptr) {
            // 读取一行
            i = 0;
            while(*ptr && *ptr != '\r' && *ptr != '\n') {
                line[i++] = *ptr++;
            }
            line[i] = '\0';
            
            // 跳过换行符
            if(*ptr == '\r') ptr++;
            if(*ptr == '\n') ptr++;
            
            // 检查是否是节名
            if(line[0] == '[') {
                in_ratio_section = (strncmp(line, "[Ratio]", 7) == 0);
                in_limit_section = (strncmp(line, "[Limit]", 7) == 0);
                continue;
            }
            
            // 解析键值对
            if(in_ratio_section && strncmp(line, "Ch0", 3) == 0) {
                char* value_start = strchr(line, '=');
                if(value_start) {
                    // 跳过等号和空格
                    value_start++;
                    while(*value_start == ' ') value_start++;
                    strcpy(ratio_value, value_start);
                    ratio_found = 1;
                }
            }
            else if(in_limit_section && strncmp(line, "Ch0", 3) == 0) {
                char* value_start = strchr(line, '=');
                if(value_start) {
                    // 跳过等号和空格
                    value_start++;
                    while(*value_start == ' ') value_start++;
                    strcpy(limit_value, value_start);
                    limit_found = 1;
                }
            }
        }
        
        // 关闭文件
        f_close(&file);
        
        // 卸载文件系统
        f_mount(0, NULL);
        
        // 输出结果
        if(ratio_found) {
            my_printf(DEBUG_USART, "Ratio=%s\r\n", ratio_value);
        }
        
        if(limit_found) {
            my_printf(DEBUG_USART, "Limit=%s\r\n", limit_value);
        }

        if(ratio_found && limit_found)
        {
            my_printf(DEBUG_USART, "config read success\r\n");
        }
    }
}

// 通用配置值设置函数
static void config_value_set(const char *cmd_name, uint8_t *cmd)
{
    if(strncmp((char*)cmd, cmd_name, strlen(cmd_name)) == 0) 
    {
        char value[32] = "1.0";  // 默认值
        float new_value;    // 新的值
        char input[32];     // 用户输入缓冲区
        char invalid_msg[32];        // 无效消息，如"ratio invalid"
        char success_msg[32];        // 成功消息，如"ratio modified success"
        char display_name[32];       // 显示名称，如"Ratio"
        uint8_t is_ratio = (strcmp(cmd_name, "ratio") == 0); // 是否为ratio命令
        float max_value = is_ratio ? 100.0f : 500.0f;  // 根据命令类型设置最大值

        // 设置显示名称（首字母大写）
        strcpy(display_name, cmd_name);
        if(display_name[0] >= 'a' && display_name[0] <= 'z') {
            display_name[0] = display_name[0] - 'a' + 'A';  // 将首字母转为大写
        }
        
        // 构建相关字符串
        sprintf(invalid_msg, "%s invalid", cmd_name);
        sprintf(success_msg, "%s modified success", cmd_name);
        
        // 从Flash读取当前值到全局变量
        uint32_t addr = 0x000000; // Flash起始地址
        uint8_t read_buffer[256] = {0};
        
        // 从Flash读取数据
        spi_flash_buffer_read(read_buffer, addr, 4 + 2 * sizeof(float));
        
        // 检查标识符
        if(strncmp((char*)read_buffer, "CONF", 4) != 0) {
            my_printf(DEBUG_USART, "No valid configuration found in flash\r\n");
            // 使用默认值
            g_ratio_value = 1.0f;
            g_limit_value = 1.0f;
        } else {
            // 解析值到全局变量
            uint32_t ratio_bits = 0;
            ratio_bits |= (uint32_t)read_buffer[4] << 0;
            ratio_bits |= (uint32_t)read_buffer[5] << 8;
            ratio_bits |= (uint32_t)read_buffer[6] << 16;
            ratio_bits |= (uint32_t)read_buffer[7] << 24;
            memcpy(&g_ratio_value, &ratio_bits, sizeof(float));
            
            uint32_t limit_bits = 0;
            limit_bits |= (uint32_t)read_buffer[8] << 0;
            limit_bits |= (uint32_t)read_buffer[9] << 8;
            limit_bits |= (uint32_t)read_buffer[10] << 16;
            limit_bits |= (uint32_t)read_buffer[11] << 24;
            memcpy(&g_limit_value, &limit_bits, sizeof(float));
        }
        
        // 获取当前值
        float current_value = is_ratio ? g_ratio_value : g_limit_value;
        
        // 格式化当前值为字符串
        sprintf(value, "%.1f", current_value);
        
        // 输出当前值
        my_printf(DEBUG_USART, "%s = %s\r\n", display_name, value);
        
        // 输出提示信息
        my_printf(DEBUG_USART, "Input value(0-%d):\r\n", (int)max_value);
        
        // 等待用户输入
        rx_flag = 0;
        while(!rx_flag) {
            // 等待接收完成
        }
        
        // 复制用户输入
        strncpy(input, (char*)uart_dma_buffer, sizeof(input) - 1);
        input[sizeof(input) - 1] = '\0'; // 确保字符串结束
        
        // 去除末尾的回车换行符
        int i = strlen(input);
        while(i > 0 && (input[i-1] == '\r' || input[i-1] == '\n')) {
            input[--i] = '\0';
        }
        
        // 清除接收标志
        rx_flag = 0;
        memset(uart_dma_buffer, 0, sizeof(uart_dma_buffer));
        
        // 验证输入是否为有效浮点数且在范围内
        if(!is_valid_float(input)) {
            my_printf(DEBUG_USART, "%s\r\n", invalid_msg);
            my_printf(DEBUG_USART, "%s = %s\r\n", display_name, value);
            return;
        }
        
        // 转换输入为浮点数
        new_value = str_to_float(input);
        if(new_value < 0.0f || new_value > max_value) {
            my_printf(DEBUG_USART, "%s\r\n", invalid_msg);
            my_printf(DEBUG_USART, "%s = %s\r\n", display_name, value);
            return;
        }
        
        // 更新全局变量
        if(is_ratio) {
            g_ratio_value = new_value;
        } else {
            g_limit_value = new_value;
        }
        
        // 输出成功信息
        my_printf(DEBUG_USART, "%s\r\n", success_msg);
        my_printf(DEBUG_USART, "%s = %.1f\r\n", display_name, new_value);

        // 记录配置变更日志
        if(is_ratio) {
            log_write("ratio config");
        } else {
            log_write("limit config");
        }
    }
}

// Ratio设置函数
void ratio_set(uint8_t * cmd)
{
    config_value_set("ratio", cmd);
}

// Limit设置函数
void limit_set(uint8_t * cmd)
{
    config_value_set("limit", cmd);
}

void config_save(uint8_t * cmd)
{
    if(strncmp((char*)cmd, "config save", 11) == 0)
    {
        // 输出当前全局变量值
        my_printf(DEBUG_USART, "ratio:%.1f\r\n", g_ratio_value);
        my_printf(DEBUG_USART, "limit:%.2f\r\n", g_limit_value);
        
        // 准备写入Flash的数据
        uint8_t write_buffer[256] = {0};
        uint16_t offset = 0;
        
        // 构建保存数据
        strcpy((char*)write_buffer, "CONF"); // 标识符
        offset += 4;
        
        // 保存ratio值
        uint32_t ratio_bits;
        memcpy(&ratio_bits, &g_ratio_value, sizeof(float));
        write_buffer[offset++] = (ratio_bits >> 0) & 0xFF;
        write_buffer[offset++] = (ratio_bits >> 8) & 0xFF;
        write_buffer[offset++] = (ratio_bits >> 16) & 0xFF;
        write_buffer[offset++] = (ratio_bits >> 24) & 0xFF;
        
        // 保存limit值
        uint32_t limit_bits;
        memcpy(&limit_bits, &g_limit_value, sizeof(float));
        write_buffer[offset++] = (limit_bits >> 0) & 0xFF;
        write_buffer[offset++] = (limit_bits >> 8) & 0xFF;
        write_buffer[offset++] = (limit_bits >> 16) & 0xFF;
        write_buffer[offset++] = (limit_bits >> 24) & 0xFF;
        
        // 擦除扇区
        uint32_t addr = 0x000000; // Flash起始地址
        spi_flash_sector_erase(addr);
        
        // 等待擦除完成
        spi_flash_wait_for_write_end();
        
        // 写入数据
        spi_flash_buffer_write(write_buffer, addr, offset);
        
        // 输出保存成功信息
        my_printf(DEBUG_USART, "save parameters to flash\r\n");

        // 记录配置保存日志
        log_write("config saved");
    }
}

void config_read_flash(uint8_t * cmd)
{
    if(strncmp((char*)cmd, "config read", 11) == 0)
    {
        // 从Flash读取当前值到全局变量
        uint32_t addr = 0x000000; // Flash起始地址
        uint8_t read_buffer[256] = {0};
        
        // 从Flash读取数据
        spi_flash_buffer_read(read_buffer, addr, 4 + 2 * sizeof(float));
        
        // 检查标识符
        if(strncmp((char*)read_buffer, "CONF", 4) != 0) {
            my_printf(DEBUG_USART, "No valid configuration found in flash\r\n");
            return;
        }
        
        // 解析值到全局变量
        uint32_t ratio_bits = 0;
        ratio_bits |= (uint32_t)read_buffer[4] << 0;
        ratio_bits |= (uint32_t)read_buffer[5] << 8;
        ratio_bits |= (uint32_t)read_buffer[6] << 16;
        ratio_bits |= (uint32_t)read_buffer[7] << 24;
        float ratio_value;
        memcpy(&ratio_value, &ratio_bits, sizeof(float));
        
        uint32_t limit_bits = 0;
        limit_bits |= (uint32_t)read_buffer[8] << 0;
        limit_bits |= (uint32_t)read_buffer[9] << 8;
        limit_bits |= (uint32_t)read_buffer[10] << 16;
        limit_bits |= (uint32_t)read_buffer[11] << 24;
        float limit_value;
        memcpy(&limit_value, &limit_bits, sizeof(float));
        
        // 输出读取到的配置
        my_printf(DEBUG_USART, "read parameters from flash\r\n");
        my_printf(DEBUG_USART, "ratio:%.1f\r\n", ratio_value);
        my_printf(DEBUG_USART, "limit:%.2f\r\n", limit_value);
    }
}

