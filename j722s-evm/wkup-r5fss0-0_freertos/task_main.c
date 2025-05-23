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
#include <kernel/dpl/SemaphoreP.h>
#include <kernel/dpl/TaskP.h>
#include <drivers/ipc_notify.h>
#include <drivers/ipc_rpmsg.h>
#include <drivers/soc.h>
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
#include "ipc_fw_version.h"
#include "FreeRTOS.h"
#include "task.h"

/* This example shows message exchange bewteen Linux and RTOS/NORTOS cores.
 * This example also does message exchange between the RTOS/NORTOS cores themselves
 *
 * The Linux core initiates IPC with other core's by sending it a message.
 * The other cores echo the same message to the Linux core.
 *
 * At the same time all RTOS/NORTOS cores, also send messages to each others
 * and reply back to each other. i.e all CPUs send and recevive messages from each other
 *
 * This example can very well have been NORTOS based, however for convinience
 * of creating two tasks to talk to two clients on linux side, we use FreeRTOS
 * for the same.
 */

/*
 * Remote core service end point
 *
 * pick any unique value on that core between 0..RPMESSAGE_MAX_LOCAL_ENDPT-1
 * the value need not be unique across cores
 *
 * The service names MUST match what linux is expecting
 */
/* This is used to run the echo test with linux kernel */
#define IPC_RPMESSAGE_SERVICE_PING        "ti.ipc4.ping-pong"
#define IPC_RPMESSAGE_ENDPT_PING          (13U)

/* This is used to run the echo test with user space kernel */
#define IPC_RPMESSAGE_SERVICE_CHRDEV      "rpmsg_chrdev"
#define IPC_RPMESSAGE_ENDPT_CHRDEV_PING   (14U)

/* Use by this to receive ACK messages that it sends to other RTOS cores */
#define IPC_RPMESSAGE_RNDPT_ACK_REPLY     (11U)

/* Maximum size that message can have in this example
 * RPMsg maximum size is 512 bytes in linux including the header of 16 bytes.
 * Message payload size without the header is 512 - 16 = 496
 */
#define IPC_RPMESSAGE_MAX_MSG_SIZE        (496u)

/*
 * Number of RP Message ping "servers" we will start,
 * - one for ping messages for linux kernel "sample ping" client
 * - and another for ping messages from linux "user space" client using "rpmsg char"
 */
#define IPC_RPMESSAGE_NUM_RECV_TASKS         (2u)

/* RPMessage object used to recvice messages */
RPMessage_Object gIpcRecvMsgObject[IPC_RPMESSAGE_NUM_RECV_TASKS];

/* RPMessage object used to send messages to other non-Linux remote cores */
RPMessage_Object gIpcAckReplyMsgObject;

/* Task priority, stack, stack size and task objects, these MUST be global's */
#define IPC_RPMESSAGE_TASK_PRI         (8U)
#define LPM_MCU_UART_WAKEUP_TASK_PRI   (8U)

#if defined (BUILD_C75X)
#define IPC_RPMESSAGE_TASK_STACK_SIZE  (32*1024U)
#else
#define IPC_RPMESSAGE_TASK_STACK_SIZE  (8*1024U)
#endif
#define LPM_MCU_UART_WAKEUP_TASK_STACK_SIZE   (1024U)

uint8_t gIpcTaskStack[IPC_RPMESSAGE_NUM_RECV_TASKS][IPC_RPMESSAGE_TASK_STACK_SIZE] __attribute__((aligned(32)));
TaskP_Object gIpcTask[IPC_RPMESSAGE_NUM_RECV_TASKS];

uint8_t gLpmUartWakeupTaskStack[LPM_MCU_UART_WAKEUP_TASK_STACK_SIZE] __attribute__((aligned(32)));
TaskP_Object gLpmUartWakeupTask;

/* number of iterations of message exchange to do */
uint32_t gMsgEchoCount = 100000u;
/* non-Linux cores that exchange messages among each other */
#if defined (SOC_AM64X)
uint32_t gRemoteCoreId[] = {
    CSL_CORE_ID_R5FSS0_0,
    CSL_CORE_ID_R5FSS0_1,
    CSL_CORE_ID_R5FSS1_0,
    CSL_CORE_ID_R5FSS1_1,
    CSL_CORE_ID_M4FSS0_0,
    CSL_CORE_ID_MAX /* this value indicates the end of the array */
};
static uint8_t gIpcInitiatorCoreID = CSL_CORE_ID_R5FSS0_0;
#endif

#if defined (SOC_AM62X)
uint32_t gRemoteCoreId[] = {
    CSL_CORE_ID_M4FSS0_0,
    CSL_CORE_ID_R5FSS0_0,
    CSL_CORE_ID_MAX /* this value indicates the end of the array */
};
uint8_t gMcuCoreID = CSL_CORE_ID_M4FSS0_0;
static uint8_t gIpcInitiatorCoreID = CSL_CORE_ID_R5FSS0_0;
#endif

#if defined (SOC_AM62AX)
uint32_t gRemoteCoreId[] = {
    CSL_CORE_ID_MCU_R5FSS0_0,
    CSL_CORE_ID_R5FSS0_0,
    CSL_CORE_ID_C75SS0_0,
    CSL_CORE_ID_MAX /* this value indicates the end of the array */
};
uint8_t gMcuCoreID = CSL_CORE_ID_MCU_R5FSS0_0;
static uint8_t gIpcInitiatorCoreID = CSL_CORE_ID_R5FSS0_0;
#endif

#if defined (SOC_AM62PX)
uint32_t gRemoteCoreId[] = {
    CSL_CORE_ID_MCU_R5FSS0_0,
    CSL_CORE_ID_WKUP_R5FSS0_0,
    CSL_CORE_ID_MAX /* this value indicates the end of the array */
};
uint8_t gMcuCoreID = CSL_CORE_ID_MCU_R5FSS0_0;
static uint8_t gIpcInitiatorCoreID = CSL_CORE_ID_WKUP_R5FSS0_0;
#endif

#if defined(SOC_J722S)
uint32_t gRemoteCoreId[] = {
    CSL_CORE_ID_MCU_R5FSS0_0,
    CSL_CORE_ID_WKUP_R5FSS0_0,
    CSL_CORE_ID_MAIN_R5FSS0_0,
    CSL_CORE_ID_C75SS0_0,
    CSL_CORE_ID_C75SS1_0,
    CSL_CORE_ID_MAX /* this value indicates the end of the array */
};
uint8_t gMcuCoreID = CSL_CORE_ID_MCU_R5FSS0_0;
static uint8_t gIpcInitiatorCoreID = CSL_CORE_ID_WKUP_R5FSS0_0;
#endif

volatile uint8_t gbShutdown = 0u;
volatile uint8_t gbShutdownRemotecoreID = 0u;
volatile uint8_t gIpcAckReplyMsgObjectPending = 0u;
volatile uint8_t gbSuspended = 0u;
volatile uint32_t gNumBytesRead = 0;

SemaphoreP_Object gLpmResumeSem;
SemaphoreP_Object gLpmSuspendSem;

void ipc_recv_task_main(void *args)
{
    int32_t status;
    char recvMsg[IPC_RPMESSAGE_MAX_MSG_SIZE+1]; /* +1 for NULL char in worst case */
    uint16_t recvMsgSize, remoteCoreId;
    uint32_t remoteCoreEndPt;
    RPMessage_Object *pRpmsgObj = (RPMessage_Object *)args;

    DebugP_log("[IPC RPMSG ECHO] Remote Core waiting for messages at end point %d ... !!!\r\n",
        RPMessage_getLocalEndPt(pRpmsgObj)
        );

    /* wait for messages forever in a loop */
    while(1)
    {
        /* set 'recvMsgSize' to size of recv buffer,
        * after return `recvMsgSize` contains actual size of valid data in recv buffer
        */
        recvMsgSize = IPC_RPMESSAGE_MAX_MSG_SIZE;
        status = RPMessage_recv(pRpmsgObj,
            recvMsg, &recvMsgSize,
            &remoteCoreId, &remoteCoreEndPt,
            SystemP_WAIT_FOREVER);

        if (gbShutdown == 1u)
        {
            break;
        }

        if (gbSuspended == 1u)
        {
            continue;
        }

        DebugP_assert(status==SystemP_SUCCESS);

        /* echo the same message string as reply */
        #if 0 /* not logging this so that this does not add to the latency of message exchange */
        recvMsg[recvMsgSize] = 0; /* add a NULL char at the end of message */
        DebugP_log("%s\r\n", recvMsg);
        #endif

        /* send ack to sender CPU at the sender end point */
        status = RPMessage_send(
            recvMsg, recvMsgSize,
            remoteCoreId, remoteCoreEndPt,
            RPMessage_getLocalEndPt(pRpmsgObj),
            SystemP_WAIT_FOREVER);
        DebugP_assert(status==SystemP_SUCCESS);
    }

    DebugP_log("[IPC RPMSG ECHO] Closing all drivers and going to WFI ... !!!\r\n");

    /* Close the drivers */
    Drivers_close();

    /* ACK the suspend message */
    IpcNotify_sendMsg(gbShutdownRemotecoreID, IPC_NOTIFY_CLIENT_ID_RP_MBOX, IPC_NOTIFY_RP_MBOX_SHUTDOWN_ACK, 1u);

    /* Disable interrupts */
    HwiP_disable();
#if (__ARM_ARCH_PROFILE == 'R') ||  (__ARM_ARCH_PROFILE == 'M')
    /* For ARM R and M cores*/
    __asm__ __volatile__ ("wfi"   "\n\t": : : "memory");
#endif
#if defined(BUILD_C7X)
    asm("    IDLE");
#endif
    vTaskDelete(NULL);
}

void ipc_rpmsg_send_messages()
{
    RPMessage_CreateParams createParams;
    uint32_t msg, i, numRemoteCores;
    uint64_t curTime;
    char msgBuf[IPC_RPMESSAGE_MAX_MSG_SIZE];
    int32_t status;
    uint16_t remoteCoreId, msgSize;
    uint32_t remoteCoreEndPt;

    RPMessage_CreateParams_init(&createParams);
    createParams.localEndPt = IPC_RPMESSAGE_RNDPT_ACK_REPLY;
    status = RPMessage_construct(&gIpcAckReplyMsgObject, &createParams);
    DebugP_assert(status==SystemP_SUCCESS);

    numRemoteCores = 0;
    for(i=0; gRemoteCoreId[i]!=CSL_CORE_ID_MAX; i++)
    {
        if(gRemoteCoreId[i] != IpcNotify_getSelfCoreId()) /* dont count self */
        {
            numRemoteCores++;
        }
    }

    DebugP_log("[IPC RPMSG ECHO] Message exchange started with RTOS cores !!!\r\n");

    curTime = ClockP_getTimeUsec();

    for(msg=0; msg<gMsgEchoCount; msg++)
    {
        snprintf(msgBuf, IPC_RPMESSAGE_MAX_MSG_SIZE-1, "%d", msg);
        msgBuf[IPC_RPMESSAGE_MAX_MSG_SIZE-1] = 0;
        msgSize = strlen(msgBuf) + 1; /* count the terminating char as well */

        /* send the same messages to all cores */
        for(i=0; gRemoteCoreId[i]!=CSL_CORE_ID_MAX; i++ )
        {
            if(gRemoteCoreId[i] != IpcNotify_getSelfCoreId()) /* dont send message to self */
            {
                status = RPMessage_send(
                    msgBuf, msgSize,
                    gRemoteCoreId[i], IPC_RPMESSAGE_ENDPT_CHRDEV_PING,
                    RPMessage_getLocalEndPt(&gIpcAckReplyMsgObject),
                    SystemP_WAIT_FOREVER);
                DebugP_assert(status==SystemP_SUCCESS);
            }
        }
        /* wait for response from all cores */
        for(i=0; gRemoteCoreId[i]!=CSL_CORE_ID_MAX; i++ )
        {
            if(gRemoteCoreId[i] != IpcNotify_getSelfCoreId()) /* dont send message to self */
            {
                /* set 'msgSize' to size of recv buffer,
                * after return `msgSize` contains actual size of valid data in recv buffer
                */
                msgSize = sizeof(msgBuf);

                gIpcAckReplyMsgObjectPending = 1;
                status = RPMessage_recv(&gIpcAckReplyMsgObject,
                    msgBuf, &msgSize,
                    &remoteCoreId, &remoteCoreEndPt,
                    SystemP_WAIT_FOREVER);
                if (gbShutdown == 1u)
                {
                    break;
                }
                DebugP_assert(status==SystemP_SUCCESS);
                gIpcAckReplyMsgObjectPending = 0;

            }
        }
        if (gbShutdown == 1u)
        {
            break;
        }
    }
    gIpcAckReplyMsgObjectPending = 0;

    curTime = ClockP_getTimeUsec() - curTime;

    DebugP_log("[IPC RPMSG ECHO] All echoed messages received by main core from %d remote cores !!!\r\n", numRemoteCores);
    DebugP_log("[IPC RPMSG ECHO] Messages sent to each core = %d \r\n", gMsgEchoCount);
    DebugP_log("[IPC RPMSG ECHO] Number of remote cores = %d \r\n", numRemoteCores);
    DebugP_log("[IPC RPMSG ECHO] Total execution time = %" PRId64 " usecs\r\n", curTime);
    DebugP_log("[IPC RPMSG ECHO] One way message latency = %" PRId32 " nsec\r\n",
        (uint32_t)(curTime*1000u/(gMsgEchoCount*numRemoteCores*2)));

    RPMessage_destruct(&gIpcAckReplyMsgObject);
}

void ipc_rpmsg_create_recv_tasks()
{
    int32_t status;
    RPMessage_CreateParams createParams;
    TaskP_Params taskParams;

    RPMessage_CreateParams_init(&createParams);
    createParams.localEndPt = IPC_RPMESSAGE_ENDPT_PING;
    status = RPMessage_construct(&gIpcRecvMsgObject[0], &createParams);
    DebugP_assert(status==SystemP_SUCCESS);

    RPMessage_CreateParams_init(&createParams);
    createParams.localEndPt = IPC_RPMESSAGE_ENDPT_CHRDEV_PING;
    status = RPMessage_construct(&gIpcRecvMsgObject[1], &createParams);
    DebugP_assert(status==SystemP_SUCCESS);

    /* We need to "announce" to Linux client else Linux does not know a service exists on this CPU
     * This is not mandatory to do for RTOS clients
     */
    status = RPMessage_announce(CSL_CORE_ID_A53SS0_0, IPC_RPMESSAGE_ENDPT_PING, IPC_RPMESSAGE_SERVICE_PING);
    DebugP_assert(status==SystemP_SUCCESS);

    status = RPMessage_announce(CSL_CORE_ID_A53SS0_0, IPC_RPMESSAGE_ENDPT_CHRDEV_PING, IPC_RPMESSAGE_SERVICE_CHRDEV);
    DebugP_assert(status==SystemP_SUCCESS);

    /* Create the tasks which will handle the ping service */
    TaskP_Params_init(&taskParams);
    taskParams.name = "RPMESSAGE_PING";
    taskParams.stackSize = IPC_RPMESSAGE_TASK_STACK_SIZE;
    taskParams.stack = gIpcTaskStack[0];
    taskParams.priority = IPC_RPMESSAGE_TASK_PRI;
    /* we use the same task function for echo but pass the appropiate rpmsg handle to it, to echo messages */
    taskParams.args = &gIpcRecvMsgObject[0];
    taskParams.taskMain = ipc_recv_task_main;

    status = TaskP_construct(&gIpcTask[0], &taskParams);
    DebugP_assert(status == SystemP_SUCCESS);

    TaskP_Params_init(&taskParams);
    taskParams.name = "RPMESSAGE_CHAR_PING";
    taskParams.stackSize = IPC_RPMESSAGE_TASK_STACK_SIZE;
    taskParams.stack = gIpcTaskStack[1];
    taskParams.priority = IPC_RPMESSAGE_TASK_PRI;
    /* we use the same task function for echo but pass the appropiate rpmsg handle to it, to echo messages */
    taskParams.args = &gIpcRecvMsgObject[1];
    taskParams.taskMain = ipc_recv_task_main;

    status = TaskP_construct(&gIpcTask[1], &taskParams);
    DebugP_assert(status == SystemP_SUCCESS);
}

void ipc_rp_mbox_callback(uint16_t remoteCoreId, uint16_t clientId, uint32_t msgValue, void *args)
{
    if (clientId == IPC_NOTIFY_CLIENT_ID_RP_MBOX)
    {
        if (msgValue == IPC_NOTIFY_RP_MBOX_SHUTDOWN) /* Shutdown request from the remotecore */
        {
            gbShutdown = 1u;
            gbShutdownRemotecoreID = remoteCoreId;
            RPMessage_unblock(&gIpcRecvMsgObject[0]);
            RPMessage_unblock(&gIpcRecvMsgObject[1]);

            if (gIpcAckReplyMsgObjectPending == 1u)
                RPMessage_unblock(&gIpcAckReplyMsgObject);
        }
        else if (msgValue == IPC_NOTIFY_RP_MBOX_SUSPEND_SYSTEM) /* Suspend request from Linux. This is send when suspending to MCU only LPM */
        {
            gbSuspended = 1u;
            SemaphoreP_post(&gLpmSuspendSem);
            IpcNotify_sendMsg(remoteCoreId, IPC_NOTIFY_CLIENT_ID_RP_MBOX, IPC_NOTIFY_RP_MBOX_SUSPEND_ACK, 1u);
        }
        else if (msgValue == IPC_NOTIFY_RP_MBOX_ECHO_REQUEST) /* This message is received after resuming from the MCU only LPM. */
        {
            gbSuspended = 0u;

            if (gNumBytesRead != 0u)
            {
                /* post this only for MCU UART Wakeup */
                SemaphoreP_post(&gLpmResumeSem);
            }

            IpcNotify_sendMsg(remoteCoreId, IPC_NOTIFY_CLIENT_ID_RP_MBOX, IPC_NOTIFY_RP_MBOX_ECHO_REPLY, 1u);
        }
    }
}

void ipc_rpmsg_echo_main(void *args)
{
    int32_t status;

    Drivers_open();
    Board_driversOpen();

    DebugP_log("[IPC RPMSG ECHO] Version: %s (%s %s):  \r\n", IPC_FW_VERSION, __DATE__, __TIME__);

    /* This API MUST be called by applications when its ready to talk to Linux */
    status = RPMessage_waitForLinuxReady(SystemP_WAIT_FOREVER);
    DebugP_assert(status==SystemP_SUCCESS);

    /* Register a callback for the RP_MBOX messages from the Linux remoteproc driver*/
    IpcNotify_registerClient(IPC_NOTIFY_CLIENT_ID_RP_MBOX, &ipc_rp_mbox_callback, NULL);

    /* create message receive tasks, these tasks always run and never exit */
    ipc_rpmsg_create_recv_tasks();

    /* wait for all non-Linux cores to be ready, this ensure that when we send messages below
     * they wont be lost due to rpmsg end point not created at remote core
     */
    IpcNotify_syncAll(SystemP_WAIT_FOREVER);

    /* Due to below "if" condition only one non-Linux core sends messages to all other non-Linux Cores
     * This is done mainly to show deterministic latency measurement
     */
    if( IpcNotify_getSelfCoreId() == gIpcInitiatorCoreID )
    {
        ipc_rpmsg_send_messages();
    }
    /* exit from this task, vTaskDelete() is called outside this function, so simply return */

    Board_driversClose();
    /* We dont close drivers since threads are running in background */
    Drivers_close();
}
