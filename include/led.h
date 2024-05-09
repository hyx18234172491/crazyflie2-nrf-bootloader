#include <nrf_gpio.h>
#include <pinout.h>
#include <nrf_delay.h> // 包含Nordic延时函数库4


void led_init(void);

void led_blink(uint32_t blink_delay_ms, uint32_t blink_count);