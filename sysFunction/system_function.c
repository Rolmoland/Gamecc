#include "mcu_cmic_gd32f470vet6.h"


extern __IO uint8_t rx_flag;

void system_selftest(uint8_t * cmd)
{
    // 检查是否为test命令
    if(strncmp((char*)cmd, "test", 4) == 0) {
        // 记录人机操作日志
        log_write("test command");
        uint32_t flash_id = 0;
        uint32_t tf_size = 0;
        rtc_parameter_struct rtc_time;
        sd_error_enum sd_status = SD_OK;
        
        my_printf(DEBUG_USART, "======system selftest======\r\n");
        
        // 测试Flash
        flash_id = spi_flash_read_id();
        if(flash_id != 0) {
            my_printf(DEBUG_USART, "flash............ok\r\n");
        } else {
            my_printf(DEBUG_USART, "flash............error\r\n");
        }
        
        // 测试TF卡
        sd_status = sd_init();
        if(sd_status == SD_OK) 
        {
            tf_size = sd_card_capacity_get();
            my_printf(DEBUG_USART, "TF card............ok\r\n");
        } 
        else 
        {
            my_printf(DEBUG_USART, "TF card............error\r\n"); // 修改为新的错误信息
            tf_size = 0;
        }
        
        // 获取Flash ID
        my_printf(DEBUG_USART, "flash ID:0x%X\r\n", flash_id);
        
        // 显示TF卡状态信息
        if(sd_status == SD_OK)
        {
            my_printf(DEBUG_USART, "TF card memory:%d KB\r\n", tf_size);
        } 
        else
        {
            my_printf(DEBUG_USART, "can not find TF card\r\n");
        }

        // 获取RTC时间
        rtc_current_time_get(&rtc_time);
        my_printf(DEBUG_USART, "RTC:20%02x-%02x-%02x %02x:%02x:%02x\r\n", 
                 rtc_time.year, rtc_time.month, rtc_time.date,
                 rtc_time.hour, rtc_time.minute, rtc_time.second);
        
        my_printf(DEBUG_USART, "======system selftest======\r\n");
        
        // 清除接收标志
        rx_flag = 0;
    }
}

void time_config(uint8_t * cmd)
{
    rtc_parameter_struct rtc_time;
    
    // 处理RTC Config命令 - 设置时间
    if(strncmp((char*)cmd, "RTC Config", 10) == 0) {
        // 记录人机操作日志
        log_write("rtc config command");
        // 获取当前时间作为基础
        rtc_current_time_get(&rtc_time);
        
        // 等待用户输入
        my_printf(DEBUG_USART, "Input Datetime\r\n");
        
        // 清除接收标志和缓冲区
        memset(cmd, 0, 256); // 假设缓冲区大小为256，根据实际情况调整
        rx_flag = 0;
        
        while(!rx_flag) {
            // 等待接收完成
        }
        
        // 解析用户输入的时间
        uint16_t year_full;
        uint8_t month, date, hour, minute, second;
        
        // 直接解析输入的时间字符串
        if(sscanf((char*)cmd, "%4hu-%2hhu-%2hhu %2hhu:%2hhu:%2hhu", 
                 &year_full, &month, &date, &hour, &minute, &second) == 6) {
            
            // 从4位年份中提取后两位作为BCD格式
            uint8_t year = (year_full % 100);
            // 转换为BCD格式
            year = ((year / 10) << 4) | (year % 10);
            month = ((month / 10) << 4) | (month % 10);
            date = ((date / 10) << 4) | (date % 10);
            hour = ((hour / 10) << 4) | (hour % 10);
            minute = ((minute / 10) << 4) | (minute % 10);
            second = ((second / 10) << 4) | (second % 10);
            
            // 设置RTC时间
            rtc_time.year = year;
            rtc_time.month = month;
            rtc_time.date = date;
            rtc_time.hour = hour;
            rtc_time.minute = minute;
            rtc_time.second = second;
            
            // 初始化RTC
            if(ERROR == rtc_init(&rtc_time)) {
                my_printf(DEBUG_USART, "RTC Config failed\r\n");
            } else {
                my_printf(DEBUG_USART, "RTC Config success\r\n");
                my_printf(DEBUG_USART, "Time: %04d-%02d-%02d %02d:%02d:%02d\r\n", 
                         year_full, 
                         month & 0x0F | ((month >> 4) * 10), 
                         date & 0x0F | ((date >> 4) * 10),
                         hour & 0x0F | ((hour >> 4) * 10), 
                         minute & 0x0F | ((minute >> 4) * 10), 
                         second & 0x0F | ((second >> 4) * 10));
            }
        } else {
            my_printf(DEBUG_USART, "Invalid time format\r\n");
            my_printf(DEBUG_USART, "Expected format: YYYY-MM-DD HH:MM:SS\r\n");
            my_printf(DEBUG_USART, "Received: %s\r\n", cmd); // 输出接收到的内容以便调试
        }
        
        // 清除接收标志
        rx_flag = 0;
    }
    // 处理RTC now命令 - 查询当前时间
    else if(strncmp((char*)cmd, "RTC now", 7) == 0) {
        // 记录人机操作日志
        log_write("rtc now command");

        // 获取当前RTC时间
        rtc_current_time_get(&rtc_time);
        
        // 显示当前时间 - 转换BCD为十进制显示
        my_printf(DEBUG_USART, "Current Time: %04d-%02d-%02d %02d:%02d:%02d\r\n", 
                 2000 + (rtc_time.year & 0x0F) + ((rtc_time.year >> 4) * 10),
                 (rtc_time.month & 0x0F) + ((rtc_time.month >> 4) * 10),
                 (rtc_time.date & 0x0F) + ((rtc_time.date >> 4) * 10),
                 (rtc_time.hour & 0x0F) + ((rtc_time.hour >> 4) * 10),
                 (rtc_time.minute & 0x0F) + ((rtc_time.minute >> 4) * 10),
                 (rtc_time.second & 0x0F) + ((rtc_time.second >> 4) * 10));
        
        // 清除接收标志
        rx_flag = 0;
    }
    
}

void system_init(void)
{
    // 系统初始化
    my_printf(DEBUG_USART, "====system init====\r\n");

    // 读取设备ID
    uint8_t device_id_buffer[64] = {0};
    uint32_t flash_addr = 0x001000; // 使用Flash的第二个扇区，避免与配置数据冲突
    spi_flash_buffer_read(device_id_buffer, flash_addr, 64);
    my_printf(DEBUG_USART, "%s\r\n", device_id_buffer);

    // 初始化测试其他
    my_printf(DEBUG_USART, "====system ready====\r\n");
    oled_printf(0, 0, "system idle");
}

void flash_device_id_set(void)
{
    // 设备ID字符串
    const char device_id[] = "Device_ID:2025-CIMC-2025689204";
    uint32_t id_length = strlen(device_id);
    uint32_t flash_addr = 0x001000; // 使用Flash的第二个扇区，避免与配置数据冲突
    
    // 擦除扇区
    my_printf(DEBUG_USART, "Erasing flash sector...\r\n");
    spi_flash_sector_erase(flash_addr);
    
    // 等待擦除完成
    spi_flash_wait_for_write_end();
    
    // 写入设备ID
    my_printf(DEBUG_USART, "Writing device ID to flash...\r\n");
    spi_flash_buffer_write((uint8_t*)device_id, flash_addr, id_length + 1); // +1 包含字符串结束符
    
    // 验证写入
    uint8_t read_buffer[64] = {0};
    spi_flash_buffer_read(read_buffer, flash_addr, id_length + 1);
    
    if(strcmp((char*)read_buffer, device_id) == 0) {
        my_printf(DEBUG_USART, "Device ID set successfully: %s\r\n", read_buffer);
    } else {
        my_printf(DEBUG_USART, "Failed to set device ID\r\n");
    }
}


