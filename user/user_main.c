/*
 * ESPRSSIF MIT License
 *
 * Copyright (c) 2015 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "esp_common.h"
#include "user_config.h"
#include "multi_button.h"
#include "gpio.h"
#include "led.h"
#include "spi_flash.h"

#include "common.h"
struct Button btn1;

extern void Ds18b20ReadTemp(void *pvParameters);
/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
*******************************************************************************/
uint32 user_rf_cal_sector_set(void)
{
    flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;
        case FLASH_SIZE_64M_MAP_1024_1024:
            rf_cal_sec = 2048 - 5;
            break;
        case FLASH_SIZE_128M_MAP_1024_1024:
            rf_cal_sec = 4096 - 5;
            break;
        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}
void wifi_event_handler_cb(System_Event_t *event)
{
    if (event == NULL) {
        return;
    }

    switch (event->event_id) {
        case EVENT_STAMODE_GOT_IP:
            os_printf("sta got ip ,create task and free heap size is %d\n", system_get_free_heap_size());
            user_conn_init();
            startLed(LED_BLUE ,300,100);
            break;

        case EVENT_STAMODE_CONNECTED:
            os_printf("sta connected\n");
            break;

        case EVENT_STAMODE_DISCONNECTED:
            wifi_station_connect();
            break;

        default:
            break;
    }
}


uint8_t read_button1_GPIO() 
{

	return GPIO_INPUT_GET(GPIO_ID_PIN(13));
}

void BTN1_PRESS_DOWN_Handler(void* btn)
{
    printf("BTN1_PRESS_DOWN_Handler\n");
}

void BTN1_PRESS_UP_Handler(void* btn)
{
    printf("BTN1_PRESS_UP_Handler\n");
}
void BTN1_PRESS_REPEAT_Handler(void* btn)
{
    printf("BTN1_PRESS_REPEAT_Handler\n");
}
void BTN1_SINGLE_Click_Handler(void* btn)
{
    printf("BTN1_SINGLE_Click_Handler\n");
}
void BTN1_DOUBLE_Click_Handler(void* btn)
{
    printf("BTN1_DOUBLE_Click_Handler\n");
}
void BTN1_LONG_RRESS_START_Handler(void* btn)
{
    xTaskCreate(smartconfig_task, "smartconfig_task", 256,(void*) 1, 2, NULL);
    startLed(LED_BLUE ,300,100);
    printf("BTN1_LONG_RRESS_START_Handler\n");
}
void BTN1_LONG_PRESS_HOLD_Handler(void* btn)
{
    printf("BTN1_LONG_PRESS_HOLD_Handler\n");
}
/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void user_init(void)
{  

     static uint8_t btn1_event_val;
    uint32_t pwm_duty_init[2]={145,145};

 //初始化 PWM，1000周期,pwm_duty_init占空比,3通道数
    uint32_t io_info[][3]={
        {PERIPHS_IO_MUX_MTDI_U,FUNC_GPIO12,12},
        {PERIPHS_IO_MUX_MTMS_U,FUNC_GPIO14,14}
    };

    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);//选择GPIO13配网按键
 

    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4);//选择GPIO4  继电器1
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);//选择GPIO5  继电器2
    //PIN_FUNC_SELECT(PERIPHS_IO_MUX_SD_DATA2_U, FUNC_GPIO9);//选择GPIO9  继电器3
    //PIN_FUNC_SELECT(PERIPHS_IO_MUX_SD_DATA3_U, FUNC_GPIO10);//选择GPIO10  继电器4
    button_init(&btn1, read_button1_GPIO, 0);
    button_attach(&btn1, SINGLE_CLICK,     BTN1_SINGLE_Click_Handler);
    button_attach(&btn1, DOUBLE_CLICK,     BTN1_DOUBLE_Click_Handler);
    button_attach(&btn1, LONG_RRESS_START, BTN1_LONG_RRESS_START_Handler);
    button_start(&btn1);
    printf("SDK version:%s\n", system_get_sdk_version());
        /* need to set opmode before you set config */
    wifi_set_opmode(STATION_MODE);
    register_led(LED_BLUE ,2,0);//led指示灯
    GPIO_DIS_OUTPUT(GPIO_ID_PIN(13));
    GPIO_AS_OUTPUT(GPIO_ID_PIN(4));
    GPIO_AS_OUTPUT(GPIO_ID_PIN(5));
 //开始初始化


    xTaskCreate(led_task, "led_task", 128, NULL, 2, NULL);
    xTaskCreate(button_task, "button_task", 256, NULL, 2, NULL);
    xTaskCreate(smartconfig_task, "smartconfig_task", 256, NULL, 2, NULL);
    xTaskCreate(&Ds18b20ReadTemp, "print_temperature", 256, NULL, 2, NULL);
    pwm_init(10000,pwm_duty_init,2,io_info);
}



