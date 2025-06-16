#ifndef __SYSTEM_FUNCTION_H__
#define __SYSTEM_FUNCTION_H__

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

void system_selftest(uint8_t * cmd);
void time_config(uint8_t * cmd);
void system_init(void);
void flash_device_id_set(void);

#ifdef __cplusplus
}
#endif

#endif

