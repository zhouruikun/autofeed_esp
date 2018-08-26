/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial API and implementation and/or initial documentation
 *******************************************************************************/

#include <stddef.h>
#include "esp_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "mqtt/MQTTClient.h"

#include "gpio.h"
#include "led.h"
#include "string.h"
 #include "common.h"

#define MQTT_BROKER  "106.14.226.150"  /* MQTT Broker Address*/
#define MQTT_PORT    1883             /* MQTT Port*/
uint16_t status[4]={0};
#define MQTT_CLIENT_THREAD_NAME         "mqtt_client_thread"
#define MQTT_CLIENT_THREAD_STACK_WORDS  2048
#define MQTT_CLIENT_THREAD_PRIO         8

LOCAL xTaskHandle mqttc_client_handle;

void mqtt_finish(void)
{
    if(mqttc_client_handle!=NULL)
    {  
        vTaskDelete(mqttc_client_handle);
     }
}


void wifi_event_handler_cb(System_Event_t *event);
static void messageArrived(MessageData* data)
{
    //处理两路PWM  
    char * str_rec = data->message->payload;
    printf("Message arrived: %s\n", str_rec);
    char * index;
 
    if( (index = (char *)strstr(str_rec,"pwm"))!=NULL)//pwm1=555
 
    {
        uint16_t pwm = (index[5]-'0')*100+ (index[6]-'0')*10+ (index[7]-'0');
        pwm_set_duty(pwm,index[3]-'0');
        pwm_start();
    }
    // 处理4路继电器 
    else if((index =strstr(str_rec,"relay"))!=NULL)//realy0=1
    {
        status[index[5]-'0']= (index[7]-'0');
        switch(index[5]-'0')
        {
           case 0:GPIO_OUTPUT_SET(GPIO_ID_PIN(4), (index[7]-'0'));  break;
           
           case 1:GPIO_OUTPUT_SET(GPIO_ID_PIN(5), (index[7]-'0'));  break;
            default:break;
        }
    }
    //   
    else if((index =strstr(str_rec,"feed"))!=NULL)
    {
        uint16_t time_feed = (index[5]-'0')*1000+ (index[6]-'0')*100+ (index[7]-'0')*10+ (index[8]-'0');
        pwm_set_duty(60,0);
        pwm_start();
        vTaskDelay(time_feed / portTICK_RATE_MS);  //send every 1 seconds
        pwm_set_duty(145,0);
        pwm_start();
    }
    //  处理刷新状态
    else if(strstr(str_rec,"get=")!=NULL)
    {
        
    }
}
extern     int temp ;
static void mqtt_client_thread(void* pvParameters)
{
    printf("mqtt client thread starts\n");
    MQTTClient client;
    Network network;
    unsigned char sendbuf[80], readbuf[80] = {0};
    int rc = 0, count = 0;
    MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;

    pvParameters = 0;
    NetworkInit(&network);
    MQTTClientInit(&client, &network, 30000, sendbuf, sizeof(sendbuf), readbuf, sizeof(readbuf));
    for (;;) {

            //判断是否已经获取了路由器分配的IP
            while (wifi_station_get_connect_status() != STATION_GOT_IP) {
                vTaskDelay(1000 / portTICK_RATE_MS);
                printf("mqtt client thread wait STATION_GOT_IP\n");
            }

        char* address = MQTT_BROKER;

        // if ((rc = NetworkConnect(&network, address, MQTT_PORT)) != 0) {
        //     printf("Return code from network connect is %d\n", rc);
        // }
        if ((rc = NetworkConnect(&network, address, MQTT_PORT)) != 0) {
                    printf("MClouds NetworkConnect connect is %d\n", rc);
                }

    #if defined(MQTT_TASK)

        if ((rc = MQTTStartTask(&client)) != pdPASS) {
            printf("Return code from start tasks is %d\n", rc);
        } else {
            printf("Use MQTTStartTask\n");
        }
    #endif

        connectData.MQTTVersion = 3;
        connectData.clientID.cstring = "auto_feed";

        if ((rc = MQTTConnect(&client, &connectData)) != 0) {
            printf("Return code from MQTT connect is %d\n", rc);
            startLed(LED_BLUE ,500,150);
        } else {
            printf("MQTT Connected\n");
            startLed(LED_BLUE ,2000,200);
        }

        char deciveid2[200] = DEVICE_ID;
        char deciveid[200] = DEVICE_ID;
        char* topicSub = strcat(deciveid,"/sub");
        char* topicPub = strcat(deciveid2,"/pub");

        if ((rc = MQTTSubscribe(&client, topicSub, 2, messageArrived)) != 0) {
            printf("Return code from MQTT subscribe is %d\n", rc);
        } else {
            printf("MQTT subscribe to topic \"%s\"\n",topicSub);
        }

        while (++count) {
            MQTTMessage message;
            char payload[64];
            message.qos = QOS2;
            message.retained = 0;
            message.payload = payload;
            sprintf(payload, "{\"temperture\":%d,\"io1\":%d,\"io2\":%d}", temp,status[0],status[1]);
            printf(payload);
            message.payloadlen = strlen(payload);

            if ((rc = MQTTPublish(&client, topicPub, &message)) != 0) {
                printf("Return code from MQTT publish is %d\n", rc);
                break;
            } else {
                printf("MQTT publish topic \"%s\", message number is %d\n",payload, count);
            }
            vTaskDelay(1000 / portTICK_RATE_MS);  //send every 1 seconds
        }
        network.disconnect(&network);
    }
    printf("mqtt_client_thread going to be deleted\n");
    vTaskDelete(NULL);
    return;
}

void user_conn_init(void)
{
    int ret;
    ret = xTaskCreate(mqtt_client_thread,
                      MQTT_CLIENT_THREAD_NAME,
                      MQTT_CLIENT_THREAD_STACK_WORDS,
                      NULL,
                      MQTT_CLIENT_THREAD_PRIO,
                      &mqttc_client_handle);

    if (ret != pdPASS)  {
        printf("mqtt create client thread %s failed\n", MQTT_CLIENT_THREAD_NAME);
    }
}
