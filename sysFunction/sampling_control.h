#ifndef __SAMPLING_CONTROL_H__
#define __SAMPLING_CONTROL_H__

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

void sampling_start_stop(uint8_t * cmd);
void process_sampling(void);

#ifdef __cplusplus
}
#endif

#endif

