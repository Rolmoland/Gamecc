#include "mcu_cmic_gd32f470vet6.h"

// 将BCD码转换为十进制 # BCD转十进制
static uint8_t bcd_to_dec(uint8_t bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

// 将时间转换为Unix时间戳 # 计算Unix时间戳
static uint32_t rtc_to_unix_timestamp(rtc_parameter_struct *rtc_time)
{
    // 将BCD码转换为十进制
    uint16_t year = 2000 + bcd_to_dec(rtc_time->year);
    uint8_t month = bcd_to_dec(rtc_time->month);
    uint8_t day = bcd_to_dec(rtc_time->date);
    uint8_t hour = bcd_to_dec(rtc_time->hour);
    uint8_t minute = bcd_to_dec(rtc_time->minute);
    uint8_t second = bcd_to_dec(rtc_time->second);
    
    // 计算从1970年1月1日到现在的天数
    uint32_t days = 0;
    
    // 计算从1970年到当前年份的天数
    for (uint16_t y = 1970; y < year; y++) {
        days += 365;
        if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) {
            days += 1; // 闰年多一天
        }
    }
    
    // 计算当前年份中，从1月到当前月份的天数
    uint8_t days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    // 如果是闰年，2月有29天
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
        days_in_month[2] = 29;
    }
    
    for (uint8_t m = 1; m < month; m++) {
        days += days_in_month[m];
    }
    
    // 加上当月的天数
    days += day - 1;
    
    // 计算总秒数
    uint32_t timestamp = days * 86400; // 86400 = 24 * 60 * 60
    timestamp += hour * 3600;
    timestamp += minute * 60;
    timestamp += second;
    
    return timestamp;
}

// 将浮点数转换为4字节HEX格式 # 浮点数转HEX
static void float_to_hex(float value, uint8_t *hex_buf)
{
    // 确保精确到小数点后两位
    int32_t fixed_point = (int32_t)(value * 100.0f + 0.5f); // 转换为定点数并四舍五入
    
    // 分离整数部分和小数部分
    uint16_t int_part = fixed_point / 100;
    uint16_t frac_part = fixed_point % 100;
    
    // 将小数部分转换为16位整数格式 (0-99 映射到 0-65535)
    uint16_t frac_hex = (uint16_t)((frac_part * 65536UL) / 100);
    
    // 整数部分转换为2字节HEX（高位在前）
    hex_buf[0] = (int_part >> 8) & 0xFF;
    hex_buf[1] = int_part & 0xFF;
    
    // 小数部分转换为2字节HEX（高位在前）
    hex_buf[2] = (frac_hex >> 8) & 0xFF;
    hex_buf[3] = frac_hex & 0xFF;
}

// 隐藏转换函数 # 隐藏格式转换
void hide_conversion(uint8_t * cmd)
{
    extern rtc_parameter_struct rtc_initpara; // RTC时间参数
    extern uint16_t adc_value[1]; // ADC采样值
    
    // 检查是否为hide命令
    if(strncmp((char*)cmd, "hide", 4) == 0) {
        // 获取当前RTC时间
        rtc_current_time_get(&rtc_initpara);
        
        // 将RTC时间转换为Unix时间戳
        uint32_t timestamp = rtc_to_unix_timestamp(&rtc_initpara);
        
        // 计算电压值 (假设ADC参考电压为3.3V，分辨率为12位)
        float voltage = (float)adc_value[0] * 3.3f / 4096.0f;
        
        // 将电压值转换为4字节HEX格式
        uint8_t voltage_hex[4];
        float_to_hex(voltage, voltage_hex);
        
        // 输出转换后的数据（时间戳和电压值合并为一段）
        my_printf(DEBUG_USART, "%08X%02X%02X%02X%02X\r\n", 
                 timestamp,
                 voltage_hex[0], voltage_hex[1], voltage_hex[2], voltage_hex[3]);
    }
}



