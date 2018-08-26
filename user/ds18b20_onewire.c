/*******************************************************************************
 * 函 数 名         : Ds18b20Init
 * 函数功能        : 初始化
 * 输    入         : 无
 * 输    出         : 初始化成功返回1，失败返回0
 *******************************************************************************/

#include "esp_common.h"
#include "user_config.h"
#include "multi_button.h"
#include "gpio.h"
#include "led.h"
#include "spi_flash.h"

#include "common.h"
    int temp = 0;
    uint8_t Ds18b20Init() {
    int i;

    gpio16_output_conf();
    // PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO16);
    // GPIO_OUTPUT_SET(DSPORT, 0);      //将总线拉低480us~960us
    gpio16_output_set(0);
    os_delay_us(600);        //延时642us
    // GPIO_OUTPUT_SET(DSPORT, 1); //然后拉高总线，如果DS18B20做出反应会将在15us~60us后总线拉低
    gpio16_output_set(1);
    gpio16_input_conf();
    os_delay_us(60);
    while (gpio16_input_get())  //等待DS18B20拉低总线
    {
        os_delay_us(60);
        i++;
        if (i > 5)  //等待>5MS
                {
                 
            return 0;   //初始化失败
        }
    }
    while (!gpio16_input_get());
 
    return 1;   //初始化成功
}

/*******************************************************************************
 * 函 数 名         : Ds18b20WriteByte
 * 函数功能        : 向18B20写入一个字节
 * 输    入         : com
 * 输    出         : 无
 *******************************************************************************/

void Ds18b20WriteByte(uint8_t dat) {
    int j;
    gpio16_output_conf();
    for (j = 0; j < 8; j++) {
        gpio16_output_set( 0);           //每写入一位数据之前先把总线拉低1us
        os_delay_us(10);
        gpio16_output_set( dat & 0x01);              //然后写入一个数据，从最低位开始
        os_delay_us(40); //延时68us，持续时间最少60us
        gpio16_output_set( 1); //然后释放总线，至少1us给总线恢复时间才能接着写入第二个数值
         os_delay_us(1); //延时68us，持续时间最少60us
        dat >>= 1;
    }
}
/*******************************************************************************
 * 函 数 名         : Ds18b20ReadByte
 * 函数功能        : 读取一个字节
 * 输    入         : com
 * 输    出         : 无
 *******************************************************************************/

uint8_t Ds18b20ReadByte() {
    uint8_t byte, bi;
    int i, j;
    for (j = 8; j > 0; j--) {
        gpio16_output_conf();
        gpio16_output_set( 0); //先将总线拉低1us
        os_delay_us(1);
        gpio16_input_conf();
        os_delay_us(1);
        bi = gpio16_input_get();     //读取数据，从最低位开始读取
        /*将byte左移一位，然后与上右移7位后的bi，注意移动之后移掉那位补0。*/
        byte = (byte >> 1) | (bi << 7);
        os_delay_us(45); //读取完之后等待48us再接着读取下一个数
    }
    return byte;
}
/*******************************************************************************
 * 函 数 名         : Ds18b20ChangTemp
 * 函数功能        : 让18b20开始转换温度
 * 输    入         : com
 * 输    出         : 无
 *******************************************************************************/

void Ds18b20ChangTemp() {

    Ds18b20Init();
    os_delay_us(1000);
    Ds18b20WriteByte(0xcc);     //跳过ROM操作命令      
    Ds18b20WriteByte(0x44);     //温度转换命令
    vTaskDelay(100 / portTICK_RATE_MS);  //send every 1 seconds

}
/*******************************************************************************
 * 函 数 名         : Ds18b20ReadTempCom
 * 函数功能        : 发送读取温度命令
 * 输    入         : com
 * 输    出         : 无
 *******************************************************************************/

void Ds18b20ReadTempCom() {

    Ds18b20Init();
    os_delay_us(1000);
    Ds18b20WriteByte(0xcc);  //跳过ROM操作命令
    Ds18b20WriteByte(0xbe);  //发送读取温度命令
}
/*******************************************************************************
 * 函 数 名         : Ds18b20ReadTemp
 * 函数功能        : 读取温度
 * 输    入         : com
 * 输    出         : 无
 *******************************************************************************/

int Ds18b20ReadTemp(void *pvParameters) {

    uint8_t tmh, tml;   gpio16_output_conf();
        printf("temperature task start\r\n");  
    while(1){
    Ds18b20ChangTemp();             //先写入转换命令
    Ds18b20ReadTempCom();           //然后等待转换完后发送读取温度命令
    tml = Ds18b20ReadByte();        //读取温度值共16位，先读低字节
    tmh = Ds18b20ReadByte();        //再读高字节
    temp = tmh;
    temp <<= 8;
    temp |= tml;
    temp = temp *6.25;
    vTaskDelay(1500 / portTICK_RATE_MS);  //send every 1 seconds

 
    }

}