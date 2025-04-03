/*
 *  Copyright (C) 2021-2023 Texas Instruments Incorporated
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <kernel/dpl/ClockP.h>
#include <kernel/dpl/DebugP.h>
#include <drivers/soc.h>
#include "ti_drivers_config.h" 
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
#include "FreeRTOS.h"
#include "task.h"

#define MAIN_TASK_SIZE (8192U/sizeof(configSTACK_DEPTH_TYPE))
StackType_t ebimuRecvStack[MAIN_TASK_SIZE] __attribute__((aligned(32)));
StackType_t deta10RecvStack[MAIN_TASK_SIZE] __attribute__((aligned(32)));
StackType_t ebimuFilterStack[MAIN_TASK_SIZE] __attribute__((aligned(32)));
StackType_t deta10FilterStack[MAIN_TASK_SIZE] __attribute__((aligned(32)));

StaticTask_t ebimuRecvTaskObj, deta10RecvTaskObj, ebimuFilterTaskObj, deta10FilterTaskObj;
TaskHandle_t ebimuRecvTask, deta10RecvTask, ebimuFilterTask, deta10FilterTask;

void ebimu_recv_main(void *args)
{

    while(1)
    {
        
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
    
    // deta10RecvTask = xTaskCreateStatic(
    //     deta10_recv_main,
    //     "deta10_recv",
    //     MAIN_TASK_SIZE,
    //     NULL,
    //     6,
    //     deta10RecvStack,
    //     &deta10RecvTaskObj
    // );

    // ebimuFilterTask = xTaskCreateStatic(
    //     ebimu_filter_main,
    //     "ebimu_filter",
    //     MAIN_TASK_SIZE,
    //     NULL,
    //     5,
    //     ebimuFilterStack,
    //     &ebimuFilterTaskObj
    // );

    // deta10FilterTask = xTaskCreateStatic(
    //     deta10_filter_main,
    //     "deta10_filter",
    //     MAIN_TASK_SIZE,
    //     NULL,
    //     5,
    //     deta10FilterStack,
    //     &deta10FilterTaskObj
    // );
}

void task_init(void *args)
{
    int32_t status;

    Drivers_open();
    Board_driversOpen();

    task_create();

    DebugP_log("task created\r\n");


    Board_driversClose();
    /* We dont close drivers since threads are running in background */
    Drivers_close();
}
