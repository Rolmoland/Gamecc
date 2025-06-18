/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2025/06/05
* Note:
*/

#include "mcu_cmic_gd32f470vet6.h"

extern uint16_t adc_value[1];

/**
 * @brief	Ê¹ÓÃÀàËÆprintfµÄ·½Ê½ÏÔÊ¾×Ö·û´®£¬ÏÔÊ¾6x8´óĞ¡µÄASCII×Ö·û
 * @param x  Character position on the X-axis  range£º0 - 127
 * @param y  Character position on the Y-axis  range£º0 - 3
 * ÀıÈç£ºoled_printf(0, 0, "Data = %d", dat);
 **/
int oled_printf(uint8_t x, uint8_t y, const char *format, ...)
{
  char buffer[512]; // ÁÙÊ±´æ´¢¸ñÊ½»¯ºóµÄ×Ö·û´®
  va_list arg;      // ´¦Àí¿É±ä²ÎÊı
  int len;          // ×îÖÕ×Ö·û´®³¤¶È

  va_start(arg, format);
  // °²È«µØ¸ñÊ½»¯×Ö·û´®µ½ buffer
  len = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);

  OLED_ShowStr(x, y, buffer, 8);
  return len;
}

void oled_task(void)
{
}

/* CUSTOM EDIT */

uint32_t one_to_one(uint8_t * cmd)
{
  uint32_t  a, b;
  uint32_t sum;
  if(a > cmd) sum = a;
  else if(b > cmd) sum = b;

  return sum;
}

uint8_t test(uint8_t * cmd)
{
  uint8_t a = 1;
  uint8_t b = 2;

  uint8 sum = a + b;
  
  return sum;
}

