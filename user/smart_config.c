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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "espressif/espconn.h"
#include "espressif/airkiss.h"
#include "common.h"
#include "led.h"
void wifi_event_handler_cb(System_Event_t *event);
#define server_ip "192.168.101.142"
#define server_port 9669


#define DEVICE_TYPE 		"gh_473770064565" //wechat public number
#define DEVICE_ID 			"gh_473770064565_444268127bb5811f" //model ID

#define DEFAULT_LAN_PORT 	12476

LOCAL esp_udp ssdp_udp;
LOCAL struct espconn pssdpudpconn;
LOCAL os_timer_t ssdp_time_serv;

uint8  lan_buf[200];
uint16 lan_buf_len;
uint8  udp_sent_cnt = 0;
struct station_config my_config;
const airkiss_config_t akconf =
{
	(airkiss_memset_fn)&memset,
	(airkiss_memcpy_fn)&memcpy,
	(airkiss_memcmp_fn)&memcmp,
	0,
};

LOCAL void ICACHE_FLASH_ATTR
airkiss_wifilan_time_callback(void)
{
	uint16 i;
	airkiss_lan_ret_t ret;
	
	if ((udp_sent_cnt++) >30) {
		udp_sent_cnt = 0;
		os_timer_disarm(&ssdp_time_serv);//s
		//return;
	}

	ssdp_udp.remote_port = DEFAULT_LAN_PORT;
	ssdp_udp.remote_ip[0] = 255;
	ssdp_udp.remote_ip[1] = 255;
	ssdp_udp.remote_ip[2] = 255;
	ssdp_udp.remote_ip[3] = 255;
	lan_buf_len = sizeof(lan_buf);
	ret = airkiss_lan_pack(AIRKISS_LAN_SSDP_NOTIFY_CMD,
		DEVICE_TYPE, DEVICE_ID, 0, 0, lan_buf, &lan_buf_len, &akconf);
	if (ret != AIRKISS_LAN_PAKE_READY) {
		os_printf("Pack lan packet error!");
		return;
	}
	
	ret = espconn_sendto(&pssdpudpconn, lan_buf, lan_buf_len);
	if (ret != 0) {
		os_printf("UDP send error!");
	}
	os_printf("Finish send notify!\n");
}

LOCAL void ICACHE_FLASH_ATTR
airkiss_wifilan_recv_callbk(void *arg, char *pdata, unsigned short len)
{
	uint16 i;
	remot_info* pcon_info = NULL;
		
	airkiss_lan_ret_t ret = airkiss_lan_recv(pdata, len, &akconf);
	airkiss_lan_ret_t packret;
	
	switch (ret){
	case AIRKISS_LAN_SSDP_REQ:
		espconn_get_connection_info(&pssdpudpconn, &pcon_info, 0);
		os_printf("remote ip: %d.%d.%d.%d \r\n",pcon_info->remote_ip[0],pcon_info->remote_ip[1],
			                                    pcon_info->remote_ip[2],pcon_info->remote_ip[3]);
		os_printf("remote port: %d \r\n",pcon_info->remote_port);
      
        pssdpudpconn.proto.udp->remote_port = pcon_info->remote_port;
		memcpy(pssdpudpconn.proto.udp->remote_ip,pcon_info->remote_ip,4);
		ssdp_udp.remote_port = DEFAULT_LAN_PORT;
		
		lan_buf_len = sizeof(lan_buf);
		packret = airkiss_lan_pack(AIRKISS_LAN_SSDP_RESP_CMD,
			DEVICE_TYPE, DEVICE_ID, 0, 0, lan_buf, &lan_buf_len, &akconf);
		
		if (packret != AIRKISS_LAN_PAKE_READY) {
			os_printf("Pack lan packet error!");
			return;
		}

		os_printf("\r\n\r\n");
		for (i=0; i<lan_buf_len; i++)
			os_printf("%c",lan_buf[i]);
		os_printf("\r\n\r\n");
		
		packret = espconn_sendto(&pssdpudpconn, lan_buf, lan_buf_len);
		if (packret != 0) {
			os_printf("LAN UDP Send err!");
		}
		
		break;
	default:
		os_printf("Pack is not ssdq req!%d\r\n",ret);
		break;
	}
}

void ICACHE_FLASH_ATTR
airkiss_start_discover(void)
{
	ssdp_udp.local_port = DEFAULT_LAN_PORT;
	pssdpudpconn.type = ESPCONN_UDP;
	pssdpudpconn.proto.udp = &(ssdp_udp);
	espconn_regist_recvcb(&pssdpudpconn, airkiss_wifilan_recv_callbk);
	espconn_create(&pssdpudpconn);

	os_timer_disarm(&ssdp_time_serv);
	os_timer_setfn(&ssdp_time_serv, (os_timer_func_t *)airkiss_wifilan_time_callback, NULL);
	os_timer_arm(&ssdp_time_serv, 1000, 1);//1s
}


void ICACHE_FLASH_ATTR
smartconfig_done(sc_status status, void *pdata)
{
    switch(status) {
        case SC_STATUS_WAIT:
            printf("SC_STATUS_WAIT\n");
            break;
        case SC_STATUS_FIND_CHANNEL:
            printf("SC_STATUS_FIND_CHANNEL\n");
            break;
        case SC_STATUS_GETTING_SSID_PSWD:
            printf("SC_STATUS_GETTING_SSID_PSWD\n");
            sc_type *type = pdata;
            if (*type == SC_TYPE_ESPTOUCH) {
                printf("SC_TYPE:SC_TYPE_ESPTOUCH\n");
            } else {
                printf("SC_TYPE:SC_TYPE_AIRKISS\n");
            }
            break;
        case SC_STATUS_LINK:
            printf("SC_STATUS_LINK\n");
            struct station_config *sta_conf = pdata;

			spi_flash_erase_sector(ADDR_MYCONF);
			spi_flash_write(ADDR_MYCONF*4096,(uint32 *)sta_conf,sizeof(struct station_config));

	        wifi_station_set_config(sta_conf);
			wifi_set_event_handler_cb(wifi_event_handler_cb); 
	        wifi_station_disconnect();
	        wifi_station_connect();


            break;
        case SC_STATUS_LINK_OVER:
            printf("SC_STATUS_LINK_OVER\n");
            if (pdata != NULL) {
				//SC_TYPE_ESPTOUCH
                uint8 phone_ip[4] = {0};

                memcpy(phone_ip, (uint8*)pdata, 4);
                printf("Phone ip: %d.%d.%d.%d\n",phone_ip[0],phone_ip[1],phone_ip[2],phone_ip[3]);
            } else {
				 printf("airkiss_start_discover");
            	//SC_TYPE_AIRKISS - support airkiss v2.0
				airkiss_start_discover();
			}
			button_start(&btn1);
            smartconfig_stop();
            break;
    }
	
}

// void ICACHE_FLASH_ATTR
// smartconfig_task(void *pvParameters)
// {
// 	startLed(LED_BLUE ,500,250);
// 	if(pvParameters == NULL)
// 	{
//     	spi_flash_read(ADDR_MYCONF*4096,(uint32 *)&my_config,sizeof(struct station_config));
// 		smartconfig_done(SC_STATUS_LINK,(void *)&my_config);
// 	}
// 	else
// 	{	
// 		mqtt_finish();
// 		smartconfig_start(smartconfig_done);	
// 	}
 

//     vTaskDelete(NULL);
// }

void ICACHE_FLASH_ATTR
smartconfig_task(void *pvParameters)
{
	uint32 sc_count=0;
	startLed(LED_BLUE ,300,150);
	if(pvParameters == NULL)
	{
		sc_count=0;
		spi_flash_read(ADDR_SC*4096,(uint32 *)&sc_count,sizeof(sc_count));
		printf("smartconfig_onoff sc_count=%u\r\n",sc_count);	
		if(sc_count>3)
		{
			printf("smartconfig_start sc_count=%u\r\n",sc_count);	
			sc_count=0;
			spi_flash_erase_sector(ADDR_SC);
			spi_flash_write(ADDR_SC*4096,(uint32 *)&sc_count,sizeof(sc_count));
			mqtt_finish();
			smartconfig_start(smartconfig_done);	
		}
		else
		{
			printf("connecting......");	
			
			sc_count=sc_count+1;
			spi_flash_erase_sector(ADDR_SC);
			spi_flash_write(ADDR_SC*4096,(uint32 *)&sc_count,sizeof(sc_count));

			spi_flash_read(ADDR_MYCONF*4096,(uint32 *)&my_config,sizeof(struct station_config));
			smartconfig_done(SC_STATUS_LINK,(void *)&my_config);

			vTaskDelay(3000 / portTICK_RATE_MS);  //send every 1 seconds
			sc_count=0;
			spi_flash_erase_sector(ADDR_SC);
			spi_flash_write(ADDR_SC*4096,(uint32 *)&sc_count,sizeof(sc_count));
		}
	}
	else
	{	
		printf("smartconfig_start_button");	
		mqtt_finish();
		smartconfig_stop(); 
		smartconfig_start(smartconfig_done);	
	}
 

    vTaskDelete(NULL);
}
