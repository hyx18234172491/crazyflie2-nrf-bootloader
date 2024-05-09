#include <nrf_gpio.h>
#include <pinout.h>
#include <nrf_delay.h> // 包含Nordic延时函数库4

// /root/swarm_flash/crazyflie2-nrf-bootloader/nrf51_sdk/nrf51822/Include/nrf_delay.h
void led_init(void);

void led_blink();