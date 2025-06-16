#ifndef __CONFIG_MANAGER_H__
#define __CONFIG_MANAGER_H__

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

void config_read(uint8_t * cmd);
void ratio_set(uint8_t * cmd);
void limit_set(uint8_t * cmd);
// void config_save(uint8_t * cmd);
// void config_read_flash(uint8_t * cmd);

#ifdef __cplusplus
}
#endif

#endif



