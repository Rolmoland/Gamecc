#ifndef __DATA_DISPOSE_H__
#define __DATA_DISPOSE_H__

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

// 全局变量声明 # 隐藏模式状态
extern uint8_t g_hide_mode;                 // 隐藏模式状态 # 0:正常模式 1:隐藏模式

void hide_conversion(uint8_t * cmd);

#ifdef __cplusplus
}
#endif

#endif
