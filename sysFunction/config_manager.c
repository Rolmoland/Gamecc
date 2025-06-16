#include "mcu_cmic_gd32f470vet6.h"

// 全局文件系统对象
static FATFS g_fs;          // 文件系统对象

extern __IO uint8_t rx_flag;
extern uint8_t uart_dma_buffer[512];

void read_config(uint8_t * cmd)
{
    // 检查是否为conf命令
    if(strncmp((char*)cmd, "conf", 4) == 0) {
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
        
        // 卸载文件系统
        f_mount(0, NULL);
        
        // 输出结果
        if(ratio_found) {
            my_printf(DEBUG_USART, "Ratio=%s\r\n", ratio_value);
        }
        
        if(limit_found) {
            my_printf(DEBUG_USART, "Limit=%s\r\n", limit_value);
        }
        
        if(ratio_found || limit_found) {
            my_printf(DEBUG_USART, "config read success\r\n");
        } else {
            my_printf(DEBUG_USART, "config.ini file format error\r\n");
        }
    }
}

// 函数原型声明
static uint8_t is_valid_float(const char *str);
static void config_value_set(const char *section_name, const char *cmd_name, uint8_t *cmd);

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
static void config_value_set(const char *section_name, const char *cmd_name, uint8_t *cmd)
{
    if(strncmp((char*)cmd, cmd_name, strlen(cmd_name)) == 0) {
        FIL file;           // 文件对象
        FRESULT fr;         // FatFs返回值
        char buffer[256];   // 读取缓冲区
        char line[256];     // 行缓冲区
        char value[32] = "1.0";  // 默认值
        uint8_t value_found = 0;     // 是否找到值
        UINT br, bw;        // 读取和写入的字节数
        char new_buffer[512]; // 新文件内容缓冲区
        char input[32];     // 用户输入缓冲区
        float value_float = 1.0f;    // 浮点值
        float new_value;    // 新的值
        char section_header[32];     // 节名称，如"[Ratio]"
        char section_line[32];       // 节行，如"Ch0="
        char invalid_msg[32];        // 无效消息，如"ratioinvalid"
        char success_msg[32];        // 成功消息，如"ratio modified success"
        
        // 构建相关字符串
        sprintf(section_header, "[%s]", section_name);
        strcpy(section_line, "Ch0=");
        sprintf(invalid_msg, "%sinvalid", cmd_name);
        sprintf(success_msg, "%s modified success", cmd_name);
        
        // 挂载文件系统
        fr = f_mount(0, &g_fs);
        if(fr != FR_OK) {
            my_printf(DEBUG_USART, "config.ini file not found.\r\n");
            return;
        }
        
        // 打开config.ini文件读取当前值
        fr = f_open(&file, "0:config.ini", FA_READ);
        if(fr != FR_OK) {
            // 文件不存在，使用默认值
            my_printf(DEBUG_USART, "%s=%s\r\n", cmd_name, value);
        } else {
            // 读取文件内容
            fr = f_read(&file, buffer, sizeof(buffer) - 1, &br);
            if(fr == FR_OK && br > 0) {
                buffer[br] = '\0'; // 确保字符串结束
                
                // 解析文件内容
                uint8_t in_section = 0;
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
                        in_section = (strncmp(line, section_header, strlen(section_header)) == 0);
                        continue;
                    }
                    
                    // 解析键值对
                    if(in_section && strncmp(line, section_line, strlen(section_line)) == 0) {
                        strcpy(value, line + strlen(section_line));
                        value_found = 1;
                        break;
                    }
                }
            }
            f_close(&file);
            
            // 输出当前值
            my_printf(DEBUG_USART, "%s=%s\r\n", cmd_name, value);
            value_float = atof(value);
        }
        
        // 提示用户输入新值
        my_printf(DEBUG_USART, "Input value(0~100):\r\n");
        
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
        
        // 验证输入是否为有效浮点数且在范围内
        new_value = atof(input);
        if(new_value < 0.0f || new_value > 100.0f || !is_valid_float(input)) {
            my_printf(DEBUG_USART, "%s\r\n", invalid_msg);
            my_printf(DEBUG_USART, "%s=%s\r\n", cmd_name, value);
            f_mount(0, NULL); // 卸载文件系统
            return;
        }
        
        // 打开文件准备写入
        fr = f_open(&file, "0:config.ini", FA_READ);
        if(fr != FR_OK) {
            // 文件不存在，创建新文件
            sprintf(new_buffer, "%s\r\n%s%s\r\n", section_header, section_line, input);
            
            fr = f_open(&file, "0:config.ini", FA_WRITE | FA_CREATE_ALWAYS);
            if(fr != FR_OK) {
                my_printf(DEBUG_USART, "Failed to create config file.\r\n");
                f_mount(0, NULL);
                return;
            }
            
            fr = f_write(&file, new_buffer, strlen(new_buffer), &bw);
            f_close(&file);
            
            if(fr != FR_OK) {
                my_printf(DEBUG_USART, "Write error.\r\n");
                f_mount(0, NULL);
                return;
            }
        } else {
            // 文件存在，读取内容
            fr = f_read(&file, buffer, sizeof(buffer) - 1, &br);
            f_close(&file);
            
            if(fr != FR_OK) {
                my_printf(DEBUG_USART, "Read error.\r\n");
                f_mount(0, NULL);
                return;
            }
            
            buffer[br] = '\0';
            
            // 修改值
            char *section = strstr(buffer, section_header);
            if(section) {
                // 找到节
                char *ch0_line = strstr(section, section_line);
                if(ch0_line) {
                    // 找到Ch0行，替换值
                    char *end_line = strstr(ch0_line, "\r\n");
                    if(end_line) {
                        // 构建新内容
                        int prefix_len = ch0_line - buffer + strlen(section_line);
                        strncpy(new_buffer, buffer, prefix_len);
                        new_buffer[prefix_len] = '\0';
                        strcat(new_buffer, input);
                        strcat(new_buffer, end_line);
                    } else {
                        // 行尾没有换行符，可能是文件末尾
                        int prefix_len = ch0_line - buffer + strlen(section_line);
                        strncpy(new_buffer, buffer, prefix_len);
                        new_buffer[prefix_len] = '\0';
                        strcat(new_buffer, input);
                    }
                } else {
                    // 没有Ch0行，添加一行
                    char *section_end = strstr(section, "\r\n");
                    if(section_end && section_end[2] == '[') {
                        // 下一行是新节
                        int prefix_len = section_end - buffer;
                        strncpy(new_buffer, buffer, prefix_len);
                        new_buffer[prefix_len] = '\0';
                        strcat(new_buffer, "\r\n");
                        strcat(new_buffer, section_line);
                        strcat(new_buffer, input);
                        strcat(new_buffer, section_end);
                    } else {
                        // 添加到节末尾
                        strcpy(new_buffer, buffer);
                        if(section_end) {
                            // 节后有内容
                            char *insert_pos = section_end + 2;
                            char temp[512];
                            strcpy(temp, insert_pos);
                            *insert_pos = '\0';
                            strcat(new_buffer, section_line);
                            strcat(new_buffer, input);
                            strcat(new_buffer, "\r\n");
                            strcat(new_buffer, temp);
                        } else {
                            // 节是文件末尾
                            strcat(new_buffer, "\r\n");
                            strcat(new_buffer, section_line);
                            strcat(new_buffer, input);
                            strcat(new_buffer, "\r\n");
                        }
                    }
                }
            } else {
                // 没有节，添加到文件末尾
                strcpy(new_buffer, buffer);
                if(buffer[0] != '\0' && buffer[strlen(buffer)-1] != '\n') {
                    strcat(new_buffer, "\r\n");
                }
                strcat(new_buffer, section_header);
                strcat(new_buffer, "\r\n");
                strcat(new_buffer, section_line);
                strcat(new_buffer, input);
                strcat(new_buffer, "\r\n");
            }
            
            // 写入新内容
            fr = f_open(&file, "0:config.ini", FA_WRITE | FA_CREATE_ALWAYS);
            if(fr != FR_OK) {
                my_printf(DEBUG_USART, "Failed to open config file for writing.\r\n");
                f_mount(0, NULL);
                return;
            }
            
            fr = f_write(&file, new_buffer, strlen(new_buffer), &bw);
            f_close(&file);
            
            if(fr != FR_OK) {
                my_printf(DEBUG_USART, "Write error.\r\n");
                f_mount(0, NULL);
                return;
            }
        }
        
        // 卸载文件系统
        f_mount(0, NULL);
        
        // 输出成功信息
        my_printf(DEBUG_USART, "%s\r\n", success_msg);
        my_printf(DEBUG_USART, "%s=%s\r\n", cmd_name, input);
    }
}

// Ratio设置函数
void ratio_set(uint8_t * cmd)
{
    config_value_set("Ratio", "Ratio", cmd);
}

// Limit设置函数
void limit_set(uint8_t * cmd)
{
    config_value_set("Limit", "Limit", cmd);
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
        memcpy(&ratio_value, read_buffer + 4, sizeof(float));
        
        // 解析limit值
        memcpy(&limit_value, read_buffer + 4 + sizeof(float), sizeof(float));
        
        // 输出参数
        my_printf(DEBUG_USART, "ratio: %.1f\r\n", ratio_value);
        my_printf(DEBUG_USART, "limit: %.2f\r\n", limit_value);
    }
}

