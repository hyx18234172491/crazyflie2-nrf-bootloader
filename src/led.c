#include "led.h"

void led_init(void)
{
    nrf_gpio_cfg_output(LED_PIN);
    nrf_gpio_pin_clear(LED_PIN); // 确保LED初始状态为关闭
}

void led_blink()
{
    for (int i = 0; i < 5; i++)
    {
        for (uint32_t i = 0; i < 1000; i++)
        {
            nrf_gpio_pin_set(LED_PIN); // 打开LED
        }
        for (uint32_t i = 0; i < 1000; i++)
        {
            nrf_gpio_pin_clear(LED_PIN); // 关闭LED
        }
    }
}