#include "mcu_cmic_gd32f470vet6.h"

extern uint16_t adc_value[1]; // ADC采样值 # 引用ADC采样值
extern rtc_parameter_struct rtc_initpara; // RTC时间参数 # 引用RTC时间参数
extern float g_ratio_value; // 从config_manager.c引入ratio全局变量 # 引入比例系数

uint8_t sampling_flag = 0; // 0:停止 1:运行 # 采样状态标志
uint32_t sampling_tick = 0; // 采样计时器计数 # 滴答定时器计数
uint32_t led_tick = 0; // LED计数值
uint8_t sampling_cycle = 5; // 采样周期(秒) # 采样周期设置

// 启动或停止采样 # 处理采样命令
void sampling_start_stop(uint8_t * cmd)
{
    // 检查是否为start命令
    if(strncmp((char*)cmd, "start", 5) == 0) 
    {
        sampling_flag = 1; // 设置采样状态为运行

        // 输出启动信息
        my_printf(DEBUG_USART, "Periodic Sampling\r\n");
        my_printf(DEBUG_USART, "sample cycle:%ds\r\n", sampling_cycle);
    }
    // 检查是否为stop命令
    else if(strncmp((char*)cmd, "stop", 4) == 0) 
    {
        if(sampling_flag == 1) 
        {
            sampling_flag = 0; // 设置采样状态为停止

            LED1_ON; // LED1熄灭

            // 输出停止信息
            my_printf(DEBUG_USART, "Periodic Sampling STOP\r\n");

            // OLED显示
            oled_printf(0, 0, "system idle");
            oled_printf(0, 1, "              ");
        }
    }
}

// 处理采样相关的周期性任务 # 采样处理函数
void process_sampling(void)
{
    if(sampling_flag == 0) return;

    float voltage, adjusted_voltage;

    // 计算电压值 (假设ADC参考电压为3.3V，分辨率为12位)
    voltage = (float)adc_value[0] * 3.3f / 4096.0f;
    
    // 应用ratio系数调整电压值
    adjusted_voltage = voltage * g_ratio_value;

    // LED1每秒闪烁一次
    uint32_t now_time = get_system_ms();

    if(now_time - led_tick >= 1000)
    {
        led_tick = now_time;
        LED1_TOGGLE;
    }

    // 获取当前RTC时间
    rtc_current_time_get(&rtc_initpara);

    // 在OLED上显示时间和调整后的电压
    oled_printf(0, 0, "%02x:%02x:%02x", rtc_initpara.hour, rtc_initpara.minute, rtc_initpara.second);
    oled_printf(0, 1, "%.2f V", adjusted_voltage);
    
    // 按照采样周期进行采样
    if(now_time - sampling_tick >= sampling_cycle * 1000)
    {
        sampling_tick = now_time;
        
        // 通过串口输出采样数据，使用调整后的电压值
        my_printf(DEBUG_USART, "20%02x-%02x-%02x %02x:%02x:%02x ch0=%.2fV\r\n", 
                rtc_initpara.year, rtc_initpara.month, rtc_initpara.date,
                rtc_initpara.hour, rtc_initpara.minute, rtc_initpara.second,
                adjusted_voltage);
    }
}
