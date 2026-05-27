/* SPDX-License-Identifier: MIT */
/*
 * Copyright (C) 2025 SZ DJI Technology Co., Ltd.
 *  
 * All information contained herein is, and remains, the property of DJI.
 * The intellectual and technical concepts contained herein are proprietary
 * to DJI and may be covered by U.S. and foreign patents, patents in process,
 * and protected by trade secret or copyright law.  Dissemination of this
 * information, including but not limited to data and other proprietary
 * material(s) incorporated within the information, in any form, is strictly
 * prohibited without the express written consent of DJI.
 *
 * If you receive this source code without DJI’s authorization, you may not
 * further disseminate the information, and you must immediately remove the
 * source code and notify DJI of its removal. DJI reserves the right to pursue
 * legal actions against you for any loss(es) or damage(s) caused by your
 * failure to do so.
 */

#include "freertos/FreeRTOS.h"

#include "connect_logic.h"
#include "display_logic.h"
#include "gps_logic.h"
#include "key_logic.h"
#include "light_logic.h"
#include "test_gps.h"

/**
 * @brief Main application function, performs initialization and task loop
 * 应用主函数，执行初始化和任务循环
 *
 * This function initializes the RGB light, GPS module, Bluetooth connection, 
 * and key logic in sequence, and starts a loop task for periodic operations.
 * 
 * 在此函数中，依次初始化氛围灯、GPS模块、蓝牙模块和按键逻辑，
 * 并启动一个循环任务，周期性进行操作。
 */
void app_main(void) {

    int res = 0;

    /* Initialize RGB light */
    /* 初始化氛围灯 */
    res = init_light_logic();
    if (res != 0) {
        return;
    }

    /* Initialize GPS module */
    /* 初始化 GPS 模块 */
    initSendGpsDataToCameraTask();

    vTaskDelay(pdMS_TO_TICKS(500));

    /* Initialize Bluetooth before display touch UI (CONNECT button needs BLE ready) */
    /* 在显示触摸 UI 之前初始化蓝牙（CONNECT 按钮依赖 BLE 就绪） */
    res = connect_logic_ble_init();
    if (res != 0) {
        return;
    }

    /* Initialize key logic */
    /* 初始化按键逻辑 */
    key_logic_init();

    res = display_logic_init();
    if (res != 0) {
        return;
    }

    /* 测试 GPS 推送 */
    /* Test GPS Data Push */
    // start_ble_packet_test(1);

    // ===== Subsequent logic loop =====
    // ===== 后续逻辑循环 =====
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
