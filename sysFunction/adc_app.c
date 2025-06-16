#include "mcu_cmic_gd32f470vet6.h"

extern uint16_t adc_value[1];
extern uint16_t convertarr[CONVERT_NUM];

void adc_task(void)
{
    convertarr[0] = adc_value[0];
    
    // 处理采样相关的周期性任务
    process_sampling();
}
 
