#include "mcu_cmic_gd32f470vet6.h"

extern rtc_parameter_struct rtc_initpara;

/*!
    \brief      display the current time
    \param[in]  none
    \param[out] none
    \retval     none
*/
void rtc_task(void)
{
    rtc_current_time_get(&rtc_initpara);
}

void rtc_config(uint8_t * cmd)
{
    if(strncmp(cmd, "if you want it", 14) == 0)
    {
        my_printf(DEBUG_USART, "you lai na");
    }
}
