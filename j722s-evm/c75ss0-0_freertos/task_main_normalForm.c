#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "FreeRTOS.h"
#include "task.h"

#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/HwiP.h>
#include <drivers/ipc_notify.h>
#include <drivers/soc.h>
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"

#define CALC_TASK_STACK_SIZE    (4 * 1024)
#define CALC_TASK_PRIORITY      (6)

static StackType_t gCalcTaskStack[CALC_TASK_STACK_SIZE];
static StaticTask_t gCalcTaskObj;
static TaskHandle_t gCalcTaskHandle = NULL;

volatile uint8_t gbShutdown = 0;
volatile uint16_t gbShutdownRemoteCoreID = 0;

/* IPC 콜백: Shutdown 요청 처리 */
void ipc_rp_mbox_callback(uint16_t remoteCoreId,
                          uint16_t clientId,
                          uint32_t msgValue,
                          void *args)
{
    if (clientId == IPC_NOTIFY_CLIENT_ID_RP_MBOX &&
        msgValue == IPC_NOTIFY_RP_MBOX_SHUTDOWN)
    {
        gbShutdown = 1;
        gbShutdownRemoteCoreID = remoteCoreId;
    }
}

/* 시스템 종료 처리 */
void task_deinit()
{
    Board_driversClose();
    Drivers_close();

    HwiP_disable();
#if (__ARM_ARCH_PROFILE == 'R') || (__ARM_ARCH_PROFILE == 'M')
    __asm__ volatile ("wfi" : : : "memory");
#elif defined(BUILD_C7X)
    asm(" IDLE");
#endif
}

/* 메인 태스크 로직 */
void calc_main(void *params)
{
    DebugP_log("calc_main started\r\n");

    while (!gbShutdown)
    {
        /* 실제 작업 수행 or 대기 */
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    DebugP_log("calc_main exiting due to shutdown\r\n");

    IpcNotify_sendMsg(gbShutdownRemoteCoreID,
                      IPC_NOTIFY_CLIENT_ID_RP_MBOX,
                      IPC_NOTIFY_RP_MBOX_SHUTDOWN_ACK,
                      1u);

    task_deinit();
    vTaskDelete(NULL);  // 자기 자신 삭제
}

/* FreeRTOS 방식 태스크 생성 */
void task_create()
{
    gCalcTaskHandle = xTaskCreateStatic(
        calc_main,
        "CalcMain",
        CALC_TASK_STACK_SIZE,
        NULL,
        CALC_TASK_PRIORITY,
        gCalcTaskStack,
        &gCalcTaskObj
    );

    configASSERT(gCalcTaskHandle != NULL);
}

/* 시스템 초기화 및 태스크 시작 */
void task_init(void *args)
{
    Drivers_open();
    Board_driversOpen();

    IpcNotify_registerClient(IPC_NOTIFY_CLIENT_ID_RP_MBOX,
                             &ipc_rp_mbox_callback,
                             NULL);
    IpcNotify_syncAll(SystemP_WAIT_FOREVER);

    task_create();

    DebugP_log("task created\r\n");
}
