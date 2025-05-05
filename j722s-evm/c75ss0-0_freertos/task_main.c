#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "FreeRTOS.h"
#include "task.h"
#include "dsplib.h"

#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/ClockP.h>
#include <kernel/dpl/CacheP.h>
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

typedef struct {
    uint8_t r, g, b;
} __attribute__((packed)) RGB;

#define PIXELS 262144

// /* IPC 콜백: Shutdown 요청 처리 */
// void ipc_rp_mbox_callback(uint16_t remoteCoreId,
//                           uint16_t clientId,
//                           uint32_t msgValue,
//                           void *args)
// {
//     if (clientId == IPC_NOTIFY_CLIENT_ID_RP_MBOX &&
//         msgValue == IPC_NOTIFY_RP_MBOX_SHUTDOWN)
//     {
//         gbShutdown = 1;
//         gbShutdownRemoteCoreID = remoteCoreId;
//     }
// }

/* 시스템 종료 처리 */
void task_deinit()
{
    Board_driversClose();
    Drivers_close();

    HwiP_disable();
    asm(" IDLE");
}

/* 메인 태스크 로직 */
// void calc_main(void *params)
// {
//     DebugP_log("calc_main started\r\n");

//     uint8_t *Ptr = (uint8_t *)0xA7000000;
//     RGB* rgb = (RGB*)((uint8_t*)0xA7000000 + 1);
//     uint8_t* gray = (uint8_t*)0xA7000000 + 786433;
//     uint64_t startTime, endTime;

//     int16_t R[PIXELS], G[PIXELS], B[PIXELS];
//     int16_t Rw[PIXELS], Gw[PIXELS], Bw[PIXELS];
//     int16_t RGB[PIXELS], RG[PIXELS];


//     while (1)
//     {
//         // if (gbShutdown)
//         // {
//         //     DebugP_log("??\r\n");
//         //     break;
//         // }
//         CacheP_inv((void*)Ptr, sizeof(int8_t), CacheP_TYPE_ALLD);
//         if(*Ptr == 32) {
//             CacheP_inv((void*)(Ptr+1), 786432, CacheP_TYPE_ALLD);
//             startTime = ClockP_getTimeUsec();

//             int i;
//             for (i = 0; i < 262144; i++) {
//                 gray[i] = (77 * rgb[i].r + 150 * rgb[i].g + 29 * rgb[i].b) >> 8;
//             }       
                     

//             endTime = ClockP_getTimeUsec();
//             CacheP_wb((void*)(Ptr+786433), 262144, CacheP_TYPE_ALLD);
//             *Ptr = 64;
//             CacheP_wb((void*)Ptr, sizeof(int8_t), CacheP_TYPE_ALLD);
//             DebugP_log("Complete: %llu usec\n", endTime - startTime);
//         }
//         else vTaskDelay(100);
//     }

//     // IpcNotify_sendMsg(gbShutdownRemoteCoreID,
//     //                   IPC_NOTIFY_CLIENT_ID_RP_MBOX,
//     //                   IPC_NOTIFY_RP_MBOX_SHUTDOWN_ACK,
//     //                   1u);

//     task_deinit();
//     vTaskDelete(NULL);  // 자기 자신 삭제
// }

int16_t  R[PIXELS], G[PIXELS], B[PIXELS];
int16_t  Rw[PIXELS], Gw[PIXELS], Bw[PIXELS];
int16_t  RG_tmp[PIXELS], RGB_tmp[PIXELS];

void calc_main(void *params)
{
    DebugP_log("calc_main started\r\n");

    uint8_t *Ptr       = (uint8_t *)0xA7000000;
    RGB     *rgb       = (RGB*)(Ptr + 1);
    uint8_t *gray      = Ptr + 1 + 3 * PIXELS;  // = Ptr + 786433
    uint64_t startTime, endTime;

    // ────────────── DSPLIB 초기화 (한 번만) ──────────────
    // 1) mulConstant 핸들
    DSPLIB_mulConstant_InitArgs mulInitArgs = {
        .dataSize  = PIXELS,
        .funcStyle = DSPLIB_FUNCTION_OPTIMIZED
    };
    int32_t mulHandleSize = DSPLIB_mulConstant_getHandleSize(&mulInitArgs);
    DSPLIB_kernelHandle mulHandle = malloc(mulHandleSize);
    DSPLIB_bufParams1D_t bufParams16 = {
        .data_type = DSPLIB_INT16,
        .dim_x     = PIXELS
    };
    if (DSPLIB_mulConstant_init(mulHandle, &bufParams16, &bufParams16, &mulInitArgs) != DSPLIB_SUCCESS) {
        DebugP_log("mulConstant init failed!\r\n");
        return;
    }

    // 2) add 핸들
    DSPLIB_add_InitArgs addInitArgs = {
        .dataSize  = PIXELS,
        .funcStyle = DSPLIB_FUNCTION_OPTIMIZED
    };
    int32_t addHandleSize = DSPLIB_add_getHandleSize(&addInitArgs);
    DSPLIB_kernelHandle addHandle = malloc(addHandleSize);
    if (DSPLIB_add_init(addHandle, &bufParams16, &bufParams16, &addInitArgs) != DSPLIB_SUCCESS) {
        DebugP_log("add init failed!\r\n");
        return;
    }

    int i;
    while (1)
    {
        CacheP_inv(Ptr, 1, CacheP_TYPE_ALLD);
        if (*Ptr == 32) {
            // 1) CPU ← DMA: RGB 데이터 invalidate
            CacheP_inv(rgb, 3 * PIXELS, CacheP_TYPE_ALLD);

            // 2) 채널 분리
            for (i = 0; i < PIXELS; i++) {
                R[i] = rgb[i].r;
                G[i] = rgb[i].g;
                B[i] = rgb[i].b;
            }

            startTime = ClockP_getTimeUsec();

            // 3) DSPLIB: 각 채널에 가중치 곱
            int16_t coeffR = 77, coeffG = 150, coeffB = 29;
            DSPLIB_mulConstant_exec(mulHandle, &coeffR, R,  Rw);
            DSPLIB_mulConstant_exec(mulHandle, &coeffG, G,  Gw);
            DSPLIB_mulConstant_exec(mulHandle, &coeffB, B,  Bw);

            // 4) DSPLIB: 두 단계로 합산
            DSPLIB_add_exec(addHandle, Rw, Gw, RG_tmp);
            DSPLIB_add_exec(addHandle, RG_tmp, Bw, RGB_tmp);

            // 5) 시프트 → 8비트 그레이스케일
            for (i = 0; i < PIXELS; i++) {
                gray[i] = (uint8_t)(RGB_tmp[i] >> 8);
            }

            endTime = ClockP_getTimeUsec();

            // 6) DMA로 결과 반영: gray 영역 write-back
            CacheP_wb(gray, PIXELS, CacheP_TYPE_ALLD);

            // 7) 완료 플래그 및 캐시 flush
            *Ptr = 64;
            CacheP_wb(Ptr, 1, CacheP_TYPE_ALLD);

            DebugP_log("Complete: %llu usec\n", endTime - startTime);
        }
        else
            vTaskDelay(100);
    }

    // (절대 도달하지 않음)
    free(mulHandle);
    free(addHandle);
    vTaskDelete(NULL);
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
}

/* 시스템 초기화 및 태스크 시작 */
void task_init(void *args)
{
    Drivers_open();
    Board_driversOpen();

    // RPMessage_waitForLinuxReady(SystemP_WAIT_FOREVER);
    // IpcNotify_registerClient(IPC_NOTIFY_CLIENT_ID_RP_MBOX,
    //                          &ipc_rp_mbox_callback,
    //                          NULL);
    // IpcNotify_syncAll(SystemP_WAIT_FOREVER);

    task_create();

    DebugP_log("task created\r\n");
}
