#include "mcu_cmic_gd32f470vet6.h"

// 全局文件系统对象
static FATFS g_fs;          // 文件系统对象

extern __IO uint8_t rx_flag;
extern uint8_t uart_dma_buffer[512];

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

void read_config(uint8_t * cmd)
{
    // 检查是否为conf命令
    if(strncmp((char*)cmd, "conf", 4) == 0) 
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
        
        // 如果找到了比率和限制值，则保存到Flash
        if(ratio_found && limit_found) {
            my_printf(DEBUG_USART, "config read success\r\n");
            
            // 保存到Flash
            uint32_t addr = 0x000000; // Flash起始地址
            uint8_t write_buffer[256] = {0};
            uint16_t offset = 0;
            
            // 构建保存数据
            strcpy((char*)write_buffer, "CONF"); // 标识符
            offset += 4;
            
            // 使用自定义函数转换ratio值
            float ratio_float = str_to_float(ratio_value);
            
            // 将浮点数转换为IEEE-754格式并存储
            uint32_t ratio_bits;
            memcpy(&ratio_bits, &ratio_float, sizeof(float));
            write_buffer[offset++] = (ratio_bits >> 0) & 0xFF;
            write_buffer[offset++] = (ratio_bits >> 8) & 0xFF;
            write_buffer[offset++] = (ratio_bits >> 16) & 0xFF;
            write_buffer[offset++] = (ratio_bits >> 24) & 0xFF;
            
            // 使用自定义函数转换limit值
            float limit_float = str_to_float(limit_value);
            
            // 将浮点数转换为IEEE-754格式并存储
            uint32_t limit_bits;
            memcpy(&limit_bits, &limit_float, sizeof(float));
            write_buffer[offset++] = (limit_bits >> 0) & 0xFF;
            write_buffer[offset++] = (limit_bits >> 8) & 0xFF;
            write_buffer[offset++] = (limit_bits >> 16) & 0xFF;
            write_buffer[offset++] = (limit_bits >> 24) & 0xFF;
            
            // 擦除扇区
            spi_flash_sector_erase(addr);
            
            // 等待擦除完成
            spi_flash_wait_for_write_end();
            
            // 写入数据
            spi_flash_buffer_write(write_buffer, addr, offset);
            
            my_printf(DEBUG_USART, "Parameters saved to flash\r\n");
        } else {
            my_printf(DEBUG_USART, "config.ini file format error\r\n");
        }
        
        // 确保在函数结束时重置接收标志和清空接收缓冲区
        rx_flag = 0;
        memset(uart_dma_buffer, 0, sizeof(uart_dma_buffer));
    }
}

// 函数原型声明
static uint8_t is_valid_float(const char *str);
static void config_value_set(const char *cmd_name, uint8_t *cmd);

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

// 通用配置值设置函数
static void config_value_set(const char *cmd_name, uint8_t *cmd)
{
    if(strncmp((char*)cmd, cmd_name, strlen(cmd_name)) == 0) 
    {
        char value[32] = "1.0";  // 默认值
        float current_value = 1.0f;    // 当前值
        float new_value;    // 新的值
        char input[32];     // 用户输入缓冲区
        char invalid_msg[32];        // 无效消息，如"ratio invalid"
        char success_msg[32];        // 成功消息，如"ratio modified success"
        char display_name[32];       // 显示名称，如"Ratio"
        uint8_t is_ratio = (strcmp(cmd_name, "ratio") == 0); // 是否为ratio命令

        // 设置显示名称（首字母大写）
        strcpy(display_name, cmd_name);
        if(display_name[0] >= 'a' && display_name[0] <= 'z') {
            display_name[0] = display_name[0] - 'a' + 'A';  // 将首字母转为大写
        }
        
        // 构建相关字符串
        sprintf(invalid_msg, "%s invalid", cmd_name);
        sprintf(success_msg, "%s modified success", cmd_name);
        
        // 从Flash读取当前值
        uint32_t addr = 0x000000; // Flash起始地址
        uint8_t read_buffer[256] = {0};
        
        // 从Flash读取数据
        spi_flash_buffer_read(read_buffer, addr, 4 + 2 * sizeof(float));
        
        // 检查标识符
        if(strncmp((char*)read_buffer, "CONF", 4) != 0) {
            my_printf(DEBUG_USART, "No valid configuration found in flash\r\n");
            my_printf(DEBUG_USART, "%s = %s\r\n", display_name, value);
        } else {
            // 解析值
            uint32_t value_bits = 0;
            if(is_ratio) {
                // 解析ratio值
                value_bits |= (uint32_t)read_buffer[4] << 0;
                value_bits |= (uint32_t)read_buffer[5] << 8;
                value_bits |= (uint32_t)read_buffer[6] << 16;
                value_bits |= (uint32_t)read_buffer[7] << 24;
            } else {
                // 解析limit值
                value_bits |= (uint32_t)read_buffer[8] << 0;
                value_bits |= (uint32_t)read_buffer[9] << 8;
                value_bits |= (uint32_t)read_buffer[10] << 16;
                value_bits |= (uint32_t)read_buffer[11] << 24;
            }
            memcpy(&current_value, &value_bits, sizeof(float));
            
            // 格式化当前值为字符串
            sprintf(value, "%.1f", current_value);
            
            // 输出当前值
            my_printf(DEBUG_USART, "%s = %s\r\n", display_name, value);
        }
        
        // 输出提示信息
        my_printf(DEBUG_USART, "Input value(0-100):\r\n");
        
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
        if(new_value < 0.0f || new_value > 100.0f) {
            my_printf(DEBUG_USART, "%s\r\n", invalid_msg);
            my_printf(DEBUG_USART, "%s = %s\r\n", display_name, value);
            return;
        }
        
        // 准备写入Flash
        uint8_t write_buffer[256] = {0};
        uint16_t offset = 0;
        
        // 构建保存数据
        strcpy((char*)write_buffer, "CONF"); // 标识符
        offset += 4;
        
        // 读取当前Flash内容
        float ratio_value = current_value;
        float limit_value = current_value;
        
        if(strncmp((char*)read_buffer, "CONF", 4) == 0) {
            // 如果Flash中有有效数据，读取现有值
            uint32_t ratio_bits = 0;
            ratio_bits |= (uint32_t)read_buffer[4] << 0;
            ratio_bits |= (uint32_t)read_buffer[5] << 8;
            ratio_bits |= (uint32_t)read_buffer[6] << 16;
            ratio_bits |= (uint32_t)read_buffer[7] << 24;
            memcpy(&ratio_value, &ratio_bits, sizeof(float));
            
            uint32_t limit_bits = 0;
            limit_bits |= (uint32_t)read_buffer[8] << 0;
            limit_bits |= (uint32_t)read_buffer[9] << 8;
            limit_bits |= (uint32_t)read_buffer[10] << 16;
            limit_bits |= (uint32_t)read_buffer[11] << 24;
            memcpy(&limit_value, &limit_bits, sizeof(float));
        }
        
        // 更新要修改的值
        if(is_ratio) {
            ratio_value = new_value;
        } else {
            limit_value = new_value;
        }
        
        // 保存ratio值
        uint32_t ratio_bits;
        memcpy(&ratio_bits, &ratio_value, sizeof(float));
        write_buffer[offset++] = (ratio_bits >> 0) & 0xFF;
        write_buffer[offset++] = (ratio_bits >> 8) & 0xFF;
        write_buffer[offset++] = (ratio_bits >> 16) & 0xFF;
        write_buffer[offset++] = (ratio_bits >> 24) & 0xFF;
        
        // 保存limit值
        uint32_t limit_bits;
        memcpy(&limit_bits, &limit_value, sizeof(float));
        write_buffer[offset++] = (limit_bits >> 0) & 0xFF;
        write_buffer[offset++] = (limit_bits >> 8) & 0xFF;
        write_buffer[offset++] = (limit_bits >> 16) & 0xFF;
        write_buffer[offset++] = (limit_bits >> 24) & 0xFF;
        
        // 擦除扇区
        spi_flash_sector_erase(addr);
        
        // 等待擦除完成
        spi_flash_wait_for_write_end();
        
        // 写入数据
        spi_flash_buffer_write(write_buffer, addr, offset);
        
        // 输出成功信息
        my_printf(DEBUG_USART, "%s\r\n", success_msg);
        my_printf(DEBUG_USART, "%s = %s\r\n", display_name, input);
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

// 保存配置到Flash
void config_save(uint8_t * cmd)
{
    // 检查是否为config_save命令
    if(strncmp((char*)cmd, "config_save", 11) == 0) {
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
            if(in_ratio_section && strncmp(line, "Ch0=", 4) == 0) {
                strcpy(ratio_value, line + 4);
                ratio_found = 1;
            }
            else if(in_limit_section && strncmp(line, "Ch0=", 4) == 0) {
                strcpy(limit_value, line + 4);
                limit_found = 1;
            }
        }
        
        // 关闭文件
        f_close(&file);
        f_mount(0, NULL);
        
        // 输出当前参数
        if(ratio_found) {
            my_printf(DEBUG_USART, "ratio: %s\r\n", ratio_value);
        } else {
            my_printf(DEBUG_USART, "ratio: not found\r\n");
            return;
        }
        
        if(limit_found) {
            my_printf(DEBUG_USART, "limit: %s\r\n", limit_value);
        } else {
            my_printf(DEBUG_USART, "limit: not found\r\n");
            return;
        }
        
        // 保存到Flash
        uint32_t addr = 0x000000; // Flash起始地址
        uint8_t write_buffer[256] = {0};
        uint16_t offset = 0;
        
        // 构建保存数据
        strcpy((char*)write_buffer, "CONF"); // 标识符
        offset += 4;
        
        // 保存ratio值
        float ratio_float = atof(ratio_value);
        memcpy(write_buffer + offset, &ratio_float, sizeof(float));
        offset += sizeof(float);
        
        // 保存limit值
        float limit_float = atof(limit_value);
        memcpy(write_buffer + offset, &limit_float, sizeof(float));
        offset += sizeof(float);
        
        // 擦除扇区
        spi_flash_sector_erase(addr);
        
        // 写入数据
        spi_flash_buffer_write(write_buffer, addr, offset);
        
        my_printf(DEBUG_USART, "save parameters to flash\r\n");
    }
}

// 从Flash读取配置
void config_read_flash(uint8_t * cmd)
{
    // 检查是否为config read命令
    if(strncmp((char*)cmd, "config read", 11) == 0) {
        uint32_t addr = 0x000000; // Flash起始地址
        uint8_t read_buffer[256] = {0};
        float ratio_value, limit_value;
        
        // 从Flash读取数据
        spi_flash_buffer_read(read_buffer, addr, 4 + 2 * sizeof(float));
        
        // 检查标识符
        if(strncmp((char*)read_buffer, "CONF", 4) != 0) {
            my_printf(DEBUG_USART, "No valid configuration found in flash\r\n");
            return;
        }
        
        // 解析ratio值
        uint32_t ratio_bits = 0;
        ratio_bits |= (uint32_t)read_buffer[4] << 0;
        ratio_bits |= (uint32_t)read_buffer[5] << 8;
        ratio_bits |= (uint32_t)read_buffer[6] << 16;
        ratio_bits |= (uint32_t)read_buffer[7] << 24;
        memcpy(&ratio_value, &ratio_bits, sizeof(float));
        
        // 解析limit值
        uint32_t limit_bits = 0;
        limit_bits |= (uint32_t)read_buffer[8] << 0;
        limit_bits |= (uint32_t)read_buffer[9] << 8;
        limit_bits |= (uint32_t)read_buffer[10] << 16;
        limit_bits |= (uint32_t)read_buffer[11] << 24;
        memcpy(&limit_value, &limit_bits, sizeof(float));
        
        // 输出参数
        my_printf(DEBUG_USART, "ratio: %.1f\r\n", ratio_value);
        my_printf(DEBUG_USART, "limit: %.2f\r\n", limit_value);
        
        // 重置接收标志和清空接收缓冲区
        rx_flag = 0;
        memset(uart_dma_buffer, 0, sizeof(uart_dma_buffer));
    }
}

