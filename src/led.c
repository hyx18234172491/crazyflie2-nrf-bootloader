#include "led.h"

void delay_ms(uint32_t milliseconds) {
    nrf_delay_ms(milliseconds); // 使用Nordic的延时函数
}

void led_init(void) {
    nrf_gpio_cfg_output(LED_PIN);
    nrf_gpio_pin_clear(LED_PIN); // 确保LED初始状态为关闭
}

void led_blink(uint32_t blink_delay_ms, uint32_t blink_count) {
    for (uint32_t i = 0; i < blink_count; i++) {
        nrf_gpio_pin_set(LED_PIN); // 打开LED
        delay_ms(blink_delay_ms);  // 持续一段时间

        nrf_gpio_pin_clear(LED_PIN); // 关闭LED
        delay_ms(blink_delay_ms);    // 持续一段时间
    }
}