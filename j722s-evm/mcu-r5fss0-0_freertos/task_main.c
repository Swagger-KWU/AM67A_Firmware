
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/ClockP.h>
#include <kernel/dpl/CacheP.h>
#include <drivers/soc.h>
#include "ti_drivers_config.h" 
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
#include "FreeRTOS.h"
#include "task.h"

#include <drivers/ipc_notify.h>
#include <drivers/ipc_rpmsg.h>
#include "ipc_fw_version.h"

#define MAIN_TASK_SIZE (8192U/sizeof(configSTACK_DEPTH_TYPE))
StackType_t ebimuRecvStack[MAIN_TASK_SIZE] __attribute__((aligned(32)));
StackType_t deta10RecvStack[MAIN_TASK_SIZE] __attribute__((aligned(32)));
StackType_t ebimuFilterStack[MAIN_TASK_SIZE] __attribute__((aligned(32)));
StackType_t deta10FilterStack[MAIN_TASK_SIZE] __attribute__((aligned(32)));

StaticTask_t ebimuRecvTaskObj, deta10RecvTaskObj, ebimuFilterTaskObj, deta10FilterTaskObj;
TaskHandle_t ebimuRecvTask, deta10RecvTask, ebimuFilterTask, deta10FilterTask;

const char* commands[9] = {
    "<soc2>",   // HEX 출력
    "<sof1>",   // Euler Angles
    "<sog1>",   // Gyro
    "<sem1>",   // Magnetometer
    "<soa2>",   // Linear Accel (Local)
    "<sot0>",   // Temp
    "<sod0>",   // Distance
    "<sots1>",  // Timestamp
    "<sor10>",  // 100Hz 출력
};

typedef struct __attribute__((packed)) {
    uint16_t sop;        // Start of packet (always 0x5555)
    
    int16_t roll;        // Euler angle - Roll (deg * 100)
    int16_t pitch;       // Euler angle - Pitch (deg * 100)
    int16_t yaw;         // Euler angle - Yaw (deg * 100)
    
    int16_t gyro_x;      // Gyroscope X (deg/s * 10)
    int16_t gyro_y;      // Gyroscope Y
    int16_t gyro_z;      // Gyroscope Z
    
    int16_t mag_x;       // Magnetometer X (uT * 10)
    int16_t mag_y;       // Magnetometer Y
    int16_t mag_z;       // Magnetometer Z
    
    int16_t accel_x;     // Linear Acceleration X (g * 1000)
    int16_t accel_y;     // Linear Acceleration Y
    int16_t accel_z;     // Linear Acceleration Z
    
    uint16_t timestamp;  // TimeStamp (ms)
    
    uint16_t checksum;   // Checksum
} EBIMU_Packet;



void uartISR()
{

}

bool validate_checksum(const EBIMU_Packet* pkt) {
    const uint8_t* ptr = (const uint8_t*)pkt;
    uint16_t sum = 0;
    for (int i = 0; i < 28; i++) {  // checksum 제외 28바이트만 더함
        sum += ptr[i];
    }
    return (sum == pkt->checksum);
}

/* 전역 변수 추가 */
volatile uint32_t gbShutdown = 0u;


void ebimu_recv_main(void *args)
{      
    UART_Transaction trans;
    UART_Transaction_init(&trans);

    for (int i = 0; i < 9; i++) {
        trans.buf = (void*)commands[i];
        trans.count = strlen(commands[i]);  // 명령어 문자열 길이
    
        CacheP_wb(trans.buf, trans.count, CacheP_TYPE_ALL);
        UART_write(gUartHandle[CONFIG_UART0], &trans);
    
        CacheP_wbInv(trans.buf, trans.count, CacheP_TYPE_ALL);
        UART_read(gUartHandle[CONFIG_UART0], &trans);
    }

    /* RX 버퍼를 내부에서 할당하여 사용 */
    trans.buf   = (uint8_t *)0xA7000000;

    EBIMU_Packet* pkt = (EBIMU_Packet*)0xA7000000;

    DebugP_log("calling UART_read() now...\r\n");

    while(1)
    {
        /* Read 후 DMA 캐시 무효화 */
        CacheP_wbInv((void *)trans.buf, 32, CacheP_TYPE_ALL);
        trans.count = 32;
        UART_read(gUartHandle[CONFIG_UART0], &trans);
        
            // DebugP_log("status : %d\r\n", trans.status);
            // DebugP_log("size : %d\r\n", trans.count);

            // for (int i = 0; i < 8; i++) {
            //     DebugP_log("%02X ", ((uint8_t*)pkt)[i]);
            // }
            // DebugP_log("\r\n");
        

        //if(validate_checksum(pkt)) {
            float roll  = pkt->roll / 100.0f;
            float gx    = pkt->gyro_x / 10.0f;
            float mx    = pkt->mag_x   / 1000.0f;
            float ax    = pkt->accel_x   / 1000.0f;
            uint16_t ts = pkt->timestamp;

            DebugP_log("Roll: %f, Gyro: %f, Magnetate: %f, Accel: %f, Timestamp: %u\r\n", roll, gx, mx, ax, ts);
        //}
        

        if (gbShutdown == 1u)
        {
            break;
        }
    }

    vTaskDelete(NULL);
}


void deta10_recv_main(void *args)
{
    while(1)
    {
        
    }

    vTaskDelete(NULL);
}

void ebimu_filter_main(void *args)
{
    while(1)
    {
        
    }

    vTaskDelete(NULL);
}

void deta10_filter_main(void *args)
{
    while(1)
    {
        
    }

    vTaskDelete(NULL);
}

void task_deinit()
{
    /* Close the drivers */
    Board_driversClose();
    Drivers_close();

    /* Disable interrupts */
    HwiP_disable();
}

void task_create()
{
    ebimuRecvTask = xTaskCreateStatic(
        ebimu_recv_main,
        "ebimu_recv",
        MAIN_TASK_SIZE,
        NULL,
        6,
        ebimuRecvStack,
        &ebimuRecvTaskObj
    );
}

void task_init(void *args)
{
    Drivers_open();
    Board_driversOpen();

    task_create();

    DebugP_log("task created\r\n");

}

/* 콜백 함수 */
void ipc_rp_mbox_callback(uint16_t remoteCoreId, uint16_t clientId, uint32_t msgValue, void *args)
{
    if (clientId == IPC_NOTIFY_CLIENT_ID_RP_MBOX)
    {
        if (msgValue == IPC_NOTIFY_RP_MBOX_SHUTDOWN) /* Shutdown request from the remotecore */
        {
            gbShutdown = 1u;
        }
    }
}
