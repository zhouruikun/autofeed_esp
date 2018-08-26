#ifndef __COMMON_H__
#define __COMMON_H__
#ifdef __cplusplus
extern "C" {
#endif
#include "multi_button.h"
extern struct Button btn1;
void mqtt_finish(void);
void smartconfig_task(void *pvParameters);
void user_conn_init(void);
#define ADDR_MYCONF 250 //the 250 sector
#define ADDR_SC 252 //the 250 sector
#define DEVICE_TYPE 		"gh_473770064565" //wechat public number
#define DEVICE_ID 			"gh_473770064565_444268127bb5811f" //model ID
#ifdef __cplusplus
}
#endif
#endif