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

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include "gps_logic.h"
#include "board_pins.h"
#include "connect_logic.h"
#include "command_logic.h"
#include "dji_protocol_data_structures.h"

#define TAG "LOGIC_GPS"
#define GPS_PUSH_INTERVAL_MS 100

#if CONFIG_WAVESHARE_ESP32_S3_TOUCH_LCD_128
#define GPS_UART_BAUD_FINAL   9600
#define GPS_UART_BAUD_PROBE   9600
#else
#define GPS_UART_BAUD_FINAL   115200
#define GPS_UART_BAUD_PROBE   115200
#endif

// Define buffer to store received data
// 定义缓冲区用于存储接收到的数据
static char buff_t[RX_BUF_SIZE]={0};

// Initialize GPS data structure
// 初始化 GPS 数据结构
static GPS_Data_t GPS_Data;

// Counter for consecutive invalid GPS readings
// GPS连续无效次数计数器
static uint8_t gps_invalid_count = 0;

static volatile uint32_t s_gps_rx_bytes_total = 0;
static volatile int64_t s_gps_last_nmea_us = 0;

/**
 * @brief Initialize GPS data structure
 *        初始化 GPS 数据结构
 *
 * Reset all fields in GPS data structure to initial values.
 * 将 GPS 数据结构的所有字段重置为初始值。
 */
static void init_gps_data(void) {
    GPS_Data.Year = 0;
    GPS_Data.Month = 0;
    GPS_Data.Day = 0;
    GPS_Data.Hour = 0;
    GPS_Data.Minute = 0;
    GPS_Data.Second = 0.0;

    GPS_Data.Latitude = 0.0;
    GPS_Data.Lat_Indicator = 'N';
    GPS_Data.Longitude = 0.0;
    GPS_Data.Lon_Indicator = 'E';

    GPS_Data.Speed_knots = 0.0;
    GPS_Data.Course = 0.0;
    GPS_Data.Altitude = 0.0;
    GPS_Data.Num_Satellites = 0;

    GPS_Data.Velocity_North = 0.0;
    GPS_Data.Velocity_East = 0.0;
    GPS_Data.Velocity_Descend = 0.0;

    // GPS_Data.Status = 0;
    GPS_Data.RMC_Valid = 0;
    GPS_Data.GGA_Valid = 0;
    GPS_Data.RMC_Latitude = 0.0;
    GPS_Data.RMC_Longitude = 0.0;
    GPS_Data.GGA_Latitude = 0.0;
    GPS_Data.GGA_Longitude = 0.0;

    GPS_Data.Horizontal_Accuracy_mm = 0;
    GPS_Data.Vertical_Accuracy_mm = 0;
    GPS_Data.Speed_Accuracy_cms = 0;
    GPS_Data.GST_Valid = 0;
}

/**
 * @brief Check if GPS signal is found
 *        检查 GPS 是否已找到信号
 *
 * @return bool Returns true if consecutive invalid count is less than 10, false otherwise
 *              如果 GPS 连续无效次数小于10，返回 true；否则返回 false
 */
bool is_gps_found(void) {
    return (gps_invalid_count < 10);
}

/**
 * @brief Check if current GPS data is valid
 *        检查当前 GPS 数据是否有效
 *
 * @return bool Returns true if GPS status is valid, false otherwise
 *              如果 GPS 状态为有效，返回 true；否则返回 false
 */
bool is_current_gps_data_valid(void) {
    if (GPS_Data.Status == 1) {
        return true;
    }
    return false;
}

double gps_get_latitude(void) {
    return GPS_Data.Latitude;
}

double gps_get_longitude(void) {
    return GPS_Data.Longitude;
}

double gps_get_altitude(void) {
    return GPS_Data.Altitude;
}

uint8_t gps_get_num_satellites(void) {
    return GPS_Data.Num_Satellites;
}

// Store previous altitude and time for velocity calculation
// 用于存储前一时刻的高度和时间，用于计算速度
static double Previous_Altitude = 0.0;
static double Previous_Time = 0.0;

// Used to store the previous latitude and longitude for outlier removal
// 用于存储前一时刻的纬度和经度，用于剔除异常值
static double Previous_Latitude = 0.0;
static double Previous_Longitude = 0.0;

/**
 * @brief Convert NMEA format coordinates to decimal degrees
 *        将 NMEA 格式的经纬度转换为十进制度
 *
 * @param nmea NMEA format coordinate string
 *             NMEA 格式的经纬度字符串
 * @param direction Direction character ('N', 'S', 'E', 'W')
 *                  方向字符（'N', 'S', 'E', 'W'）
 *
 * @return double Converted decimal degree value
 *                转换后的十进制度值
 */
double Convert_NMEA_To_Degree(const char *nmea, char direction) {
    double deg = 0.0;
    double min = 0.0;

    // Determine integer and decimal parts
    // 确定整数部分和小数部分
    const char *dot = nmea;
    while (*dot && *dot != '.') { // Find decimal point position
                                  // 找到小数点的位置
        dot++;
    }

    // Calculate integer part
    // 计算整数部分
    int int_part = 0;
    const char *ptr = nmea;
    while (ptr < dot && isdigit((unsigned char)*ptr)) {
        int_part = int_part * 10 + (*ptr - '0'); // Accumulate digits
                                                 // 累积数字
        ptr++;
    }

    // Calculate decimal part
    // 计算小数部分
    double frac_part = 0.0;
    double divisor = 10.0;
    ptr = dot + 1; // Skip decimal point
                   // 跳过小数点
    while (*ptr && isdigit((unsigned char)*ptr)) {
        frac_part += (*ptr - '0') / divisor;
        divisor *= 10.0;
        ptr++;
    }

    // Combine integer and decimal parts
    // 合并整数部分和小数部分
    deg = int_part + frac_part;

    // Separate degrees and minutes
    // 分离度和分
    min = deg - ((int)(deg / 100)) * 100;
    deg = ((int)(deg / 100)) + min / 60.0;

    // Adjust sign based on direction
    // 根据方向调整正负
    if (direction == 'S' || direction == 'W') {
        deg = -deg;
    }

    return deg;
}

/**
 * @brief Parse GNRMC sentence, e.g.: $GNRMC,074700.000,A,2234.732734,N,11356.317512,E,1.67,285.57,150125,,,A,V*03
 *        解析 GNRMC 语句，例如：$GNRMC,074700.000,A,2234.732734,N,11356.317512,E,1.67,285.57,150125,,,A,V*03
 *
 * Parse GNRMC sentence to extract GPS data including time, status, latitude, longitude, speed, course, etc.
 * (There may be data accuracy issues that can be optimized as needed)
 * 解析 GNRMC 语句，提取 GPS 数据，包括时间、状态、纬度、经度、速度、航向等信息（可能存在数据不精确问题，可自行优化）。
 *
 * @param sentence Input GNRMC sentence string
 *                 输入的 GNRMC 语句字符串
 */
void Parse_GNRMC(char *sentence) {
    char *token = strtok(sentence, ",");
    int field = 0;

    double temp_latitude = 0.0;
    double temp_longitude = 0.0;

    while (token != NULL) {
        field++;
        switch (field) {
            case 2: {
                // 时间 hhmmss.sss
                // Time hhmmss.sss
                uint8_t hour = 0, minute = 0;
                double second = 0.0;

                // 手动解析时间
                // Manually parse time
                const char *ptr = token;
                hour = (ptr[0] - '0') * 10 + (ptr[1] - '0');
                minute = (ptr[2] - '0') * 10 + (ptr[3] - '0');
                second = atof(ptr + 4);

                GPS_Data.Hour = hour;
                GPS_Data.Minute = minute;
                GPS_Data.Second = second;
                break;
            }
            case 3:
                // 状态 A/V
                // Status A/V
                GPS_Data.RMC_Valid = (token[0] == 'A') ? 1 : 0;
                break;
            case 4:
                // 纬度（暂时存储）
                // Latitude (temporary storage)
                temp_latitude = Convert_NMEA_To_Degree(token, 'N');
                break;
            case 5:
                // 更新纬度方向
                // Update latitude direction
                GPS_Data.Lat_Indicator = token[0];
                GPS_Data.RMC_Latitude = (GPS_Data.Lat_Indicator == 'S') ? -temp_latitude : temp_latitude;
                break;
            case 6:
                // 经度（暂时存储）
                // Longitude (temporary storage)
                temp_longitude = Convert_NMEA_To_Degree(token, 'E');
                break;
            case 7:
                // 更新经度方向
                // Update longitude direction
                GPS_Data.Lon_Indicator = token[0];
                GPS_Data.RMC_Longitude = (GPS_Data.Lon_Indicator == 'W') ? -temp_longitude : temp_longitude;
                break;
            case 8:
                // 地面速度 (节)
                // Ground speed (knots)
                GPS_Data.Speed_knots = atof(token);
                break;
            case 9:
                // 航向 (度)
                // Course (degrees)
                GPS_Data.Course = atof(token);
                break;
            case 10: {
                // 日期 ddmmyy
                // Date ddmmyy
                uint8_t day = 0, month = 0, year = 0;

                // 手动解析日期
                // Manually parse date
                const char *ptr = token;
                day = (ptr[0] - '0') * 10 + (ptr[1] - '0');
                month = (ptr[2] - '0') * 10 + (ptr[3] - '0');
                year = (ptr[4] - '0') * 10 + (ptr[5] - '0');

                GPS_Data.Day = day;
                GPS_Data.Month = month;
                GPS_Data.Year = year;
                break;
            }
            default:
                break;
        }
        token = strtok(NULL, ",");
    }

    // 计算向北和向东的速度分量 (米/秒)，节转米/秒
    // Calculate velocity components to north and east (m/s), convert from knots to m/s
    double speed_m_s = GPS_Data.Speed_knots * 0.514444;
    GPS_Data.Velocity_North = speed_m_s * cos(GPS_Data.Course * M_PI / 180.0);
    GPS_Data.Velocity_East = speed_m_s * sin(GPS_Data.Course * M_PI / 180.0);
}

/**
 * @brief Parse GNGGA sentence, e.g.: $GNGGA,074700.000,2234.732734,N,11356.317512,E,1,7,1.31,47.379,M,-2.657,M,,*65
 *        解析 GNGGA 语句，例如：$GNGGA,074700.000,2234.732734,N,11356.317512,E,1,7,1.31,47.379,M,-2.657,M,,*65
 *
 * Parse GNGGA sentence to extract GPS data including time, latitude, longitude, number of satellites, altitude, etc.
 * (There may be data accuracy issues that can be optimized as needed)
 * 解析 GNGGA 语句，提取 GPS 数据，包括时间、纬度、经度、卫星数量、海拔高度等信息（可能存在数据不精确问题，可自行优化）。
 *
 * @param sentence Input GNGGA sentence string
 *                 输入的 GNGGA 语句字符串
 */
void Parse_GNGGA(char *sentence) {
    char *token = strtok(sentence, ",");
    int field = 0;

    double temp_latitude = 0.0;
    double temp_longitude = 0.0;

    while (token != NULL) {
        field++;
        switch (field) {
            case 1:
                // $GNGGA
                break;
            case 2:
                // 时间 hhmmss.sss，可与GNRMC中的时间对比，确保同步
                // Time hhmmss.sss, can be compared with GNRMC time for synchronization
                break;
            case 3:
                // 纬度
                // Latitude
                temp_latitude = Convert_NMEA_To_Degree(token, 'N');
                break;
            case 4:
                // N/S
                // North/South indicator
                GPS_Data.Lat_Indicator = token[0];
                GPS_Data.GGA_Latitude = (GPS_Data.Lat_Indicator == 'S') ? -temp_latitude : temp_latitude;
                break;
            case 5:
                // 经度
                // Longitude
                temp_longitude = Convert_NMEA_To_Degree(token, 'E');
                break;
            case 6:
                // E/W
                // East/West indicator
                GPS_Data.Lon_Indicator = token[0];
                GPS_Data.GGA_Longitude = (GPS_Data.Lon_Indicator == 'W') ? -temp_longitude : temp_longitude;
                break;
            case 7:
                // 定位质量
                // Position fix quality
                if (token[0] != '\0') {
                    int quality = atoi(token);
                    GPS_Data.GGA_Valid = (quality > 0) ? 1 : 0;
                }
                break;
            case 8:
                // 可见卫星数量
                // Number of satellites in view
                GPS_Data.Num_Satellites = (uint8_t) atoi(token);
                break;
            case 9: {
                // HDOP → horizontal/vertical accuracy fallback when GPGST absent
                double hdop = atof(token);
                if (hdop > 0.0 && !GPS_Data.GST_Valid) {
                    GPS_Data.Horizontal_Accuracy_mm = (uint32_t)(hdop * 5000.0);
                    GPS_Data.Vertical_Accuracy_mm = (uint32_t)(hdop * 7500.0);
                }
                break;
            }
            case 10:
                // 海拔高度 (米)
                // Altitude (meters)
                GPS_Data.Altitude = atof(token);
                // 计算下降速度 (需要上一高度和时间)
                // Calculate descent velocity (needs previous altitude and time)
                if (Previous_Time > 0.0) {
                    double current_time = GPS_Data.Hour * 3600 + GPS_Data.Minute * 60 + GPS_Data.Second;
                    double delta_time = current_time - Previous_Time;

                    // 处理跨天情况
                    // Handle day crossover
                    if (delta_time < -43200) {  // 如果时间差小于-12小时，说明跨天了
                                                // If time difference is less than -12 hours, day has changed
                        delta_time += 86400;    // 加上24小时
                                                // Add 24 hours
                    } else if (delta_time > 43200) {  // 如果时间差大于12小时，说明是前一天的数据
                                                      // If time difference is more than 12 hours, it's previous day's data
                        delta_time -= 86400;
                    }

                    if (delta_time > 0 && delta_time < 10) {  // 只处理合理的时间差（比如小于10秒）
                                                              // Only process reasonable time differences (e.g., less than 10 seconds)
                        double delta_altitude = GPS_Data.Altitude - Previous_Altitude;
                        // 过滤异常值（比如高度差太大）
                        // Filter abnormal values (e.g., too large altitude differences)
                        if (fabs(delta_altitude) < 100) {  // 假设最大垂直速度不超过100m/s
                                                           // Assume maximum vertical speed doesn't exceed 100m/s
                            GPS_Data.Velocity_Descend = -delta_altitude / delta_time;  // 注意符号：上升为负，下降为正
                                                                                       // Note: negative for ascent, positive for descent
                        }
                    }
                }
                Previous_Altitude = GPS_Data.Altitude;
                Previous_Time = GPS_Data.Hour * 3600 + GPS_Data.Minute * 60 + GPS_Data.Second;
                break;
            // 其他字段可根据需要解析
            // Other fields can be parsed as needed
            default:
                break;
        }
        token = strtok(NULL, ",");
    }
}

/**
 * @brief Parse GPGST sentence for position error std-dev (meters).
 *        解析 GPGST 语句，提取经纬度/高度标准差（米）。
 */
void Parse_GPGST(char *sentence) {
    char *token = strtok(sentence, ",");
    int field = 0;
    uint32_t lat_mm = 0;
    uint32_t lon_mm = 0;
    uint32_t alt_mm = 0;

    GPS_Data.GST_Valid = 0;

    while (token != NULL) {
        field++;
        switch (field) {
            case 6:
                if (token[0] != '\0') {
                    double m = atof(token);
                    if (m > 0.0) {
                        lat_mm = (uint32_t)(m * 1000.0);
                    }
                }
                break;
            case 7:
                if (token[0] != '\0') {
                    double m = atof(token);
                    if (m > 0.0) {
                        lon_mm = (uint32_t)(m * 1000.0);
                    }
                }
                break;
            case 8:
                if (token[0] != '\0') {
                    double m = atof(token);
                    if (m > 0.0) {
                        alt_mm = (uint32_t)(m * 1000.0);
                    }
                }
                break;
            default:
                break;
        }
        token = strtok(NULL, ",");
    }

    if (lat_mm > 0 || lon_mm > 0) {
        GPS_Data.Horizontal_Accuracy_mm = lat_mm > lon_mm ? lat_mm : lon_mm;
        GPS_Data.GST_Valid = 1;
        GPS_Data.Speed_Accuracy_cms = GPS_Data.Horizontal_Accuracy_mm / 100;
        if (GPS_Data.Speed_Accuracy_cms < 10) {
            GPS_Data.Speed_Accuracy_cms = 10;
        } else if (GPS_Data.Speed_Accuracy_cms > 500) {
            GPS_Data.Speed_Accuracy_cms = 500;
        }
    }
    if (alt_mm > 0) {
        GPS_Data.Vertical_Accuracy_mm = alt_mm;
        GPS_Data.GST_Valid = 1;
    }
}

static void parse_nmea_line(char *line) {
    if (strncmp(line, "$GNRMC", 6) == 0 || strncmp(line, "$GPRMC", 6) == 0) {
        Parse_GNRMC(line);
    } else if (strncmp(line, "$GNGGA", 6) == 0 || strncmp(line, "$GPGGA", 6) == 0) {
        Parse_GNGGA(line);
    } else if (strncmp(line, "$GNGST", 6) == 0 || strncmp(line, "$GPGST", 6) == 0) {
        Parse_GPGST(line);
    }
}

/**
 * @brief 解析 NMEA 缓冲区中的所有语句
 *        Parse all sentences in NMEA buffer
 *
 * 遍历缓冲区中的每一行，识别并解析 GNRMC、GNGGA、GPGST 语句。
 * Traverse each line in the buffer, identify and parse GNRMC, GNGGA, and GPGST sentences.
 *
 * @param buffer 包含 NMEA 语句的缓冲区
 *               Buffer containing NMEA sentences
 */
void Parse_NMEA_Buffer(char *buffer) {
    char *start = buffer; // 指向字符串的开始
                          // Points to the start of string
    char *end = NULL;     // 指向每行的结束位置
                          // Points to the end of each line

    while ((end = strchr(start, '\n')) != NULL) {
        size_t line_length = end - start; // 计算每行的长度
                                          // Calculate length of each line

        if (line_length > 0) {
            char line[RX_BUF_SIZE] = {0}; // 创建一个临时缓冲区存储单行
                                          // Create a temporary buffer to store single line
            strncpy(line, start, line_length); // 将该行拷贝到缓冲区
                                               // Copy the line to buffer
            line[line_length] = '\0'; // 确保以空字符结尾
                                      // Ensure null-terminated string

            parse_nmea_line(line);
        }

        start = end + 1; // 移动到下一行的开始
                         // Move to the start of next line
    }

    // 处理最后一行（如果没有以换行符结尾）
    // Process the last line (if not ending with newline)
    if (*start != '\0') {
        parse_nmea_line(start);
    }

    // 在所有语句解析完成后，更新最终状态和位置数据
    // After parsing all sentences, update final status and position data
    if (GPS_Data.RMC_Valid && GPS_Data.GGA_Valid) {
        GPS_Data.Status = 1;
        gps_invalid_count = 0;  // 重置计数器
                                // Reset counter
        // 计算平均值
        // Calculate average
        GPS_Data.Latitude = (GPS_Data.RMC_Latitude + GPS_Data.GGA_Latitude) / 2.0;
        GPS_Data.Longitude = (GPS_Data.RMC_Longitude + GPS_Data.GGA_Longitude) / 2.0;

        // 与前一时刻的纬度和经度做对比
        // Compare with previous latitude and longitude
        if (fabs(GPS_Data.Latitude - Previous_Latitude) > 0.009 || fabs(GPS_Data.Longitude - Previous_Longitude) > 0.0127) {
            // 超过阈值，剔除异常值并更新前一时刻经纬度
            // If the change exceeds threshold, set status to 0 and update the previous latitude and longitude
            GPS_Data.Status = 0;
        }

        // 更新前一时刻的经纬度
        // Update the previous latitude and longitude
        Previous_Latitude = GPS_Data.Latitude;
        Previous_Longitude = GPS_Data.Longitude;
    } else {
        GPS_Data.Status = 0;
        if (gps_invalid_count < UINT8_MAX) {  // 防止溢出
                                              // Prevent overflow
            gps_invalid_count++;
        }
    }
}

/**
 * @brief 打印当前的 GPS 数据
 *        Print current GPS data
 *
 * 将当前的 GPS 数据以日志的形式输出。
 * Output current GPS data in log format.
 */
void print_gps_data() {
    ESP_LOGI(TAG,
        "GPS Data: Time=%02d:%02d:%06.3f, Date=%02d-%02d-20%02d, "
        "Lat=%f %c, Lon=%f %c, Speed=%.2f knots, Course=%.2f deg, "
        "Altitude=%.2f m, Satellites=%d, V_North=%.2f m/s, V_East=%.2f m/s, V_Descend=%.2f m/s",
        GPS_Data.Hour, GPS_Data.Minute, GPS_Data.Second,
        GPS_Data.Day, GPS_Data.Month, GPS_Data.Year,
        GPS_Data.Latitude, GPS_Data.Lat_Indicator,
        GPS_Data.Longitude, GPS_Data.Lon_Indicator,
        GPS_Data.Speed_knots, GPS_Data.Course,
        GPS_Data.Altitude, GPS_Data.Num_Satellites,
        GPS_Data.Velocity_North, GPS_Data.Velocity_East,
        GPS_Data.Velocity_Descend
    );
}

/**
 * @brief 推送 GPS 数据到相机
 *        Push GPS data to camera
 *
 * 将当前的 GPS 数据转换为指定格式，并通过命令逻辑推送到相机。
 * Convert current GPS data to specified format and push to camera through command logic.
 */
static uint32_t gps_push_horizontal_accuracy_mm(void) {
    if (GPS_Data.Horizontal_Accuracy_mm > 0) {
        return GPS_Data.Horizontal_Accuracy_mm;
    }
    return 1000;
}

static uint32_t gps_push_vertical_accuracy_mm(void) {
    if (GPS_Data.Vertical_Accuracy_mm > 0) {
        return GPS_Data.Vertical_Accuracy_mm;
    }
    return 1000;
}

static uint32_t gps_push_speed_accuracy_cms(void) {
    if (GPS_Data.Speed_Accuracy_cms > 0) {
        return GPS_Data.Speed_Accuracy_cms;
    }
    return 10;
}

void gps_push_data(void) {
    int32_t year_month_day = (GPS_Data.Year + 2000) * 10000 + GPS_Data.Month * 100 + GPS_Data.Day;
    int32_t hour_minute_second = (GPS_Data.Hour + 8) * 10000 + GPS_Data.Minute * 100 + (int32_t)GPS_Data.Second;
    int32_t gps_longitude = (int32_t)(GPS_Data.Longitude * 1e7);
    int32_t gps_latitude = (int32_t)(GPS_Data.Latitude * 1e7);
    int32_t height = (int32_t)(GPS_Data.Altitude * 1000);
    float speed_to_north = (float)(GPS_Data.Velocity_North * 100.0);
    float speed_to_east = (float)(GPS_Data.Velocity_East * 100.0);
    float speed_to_wnward = (float)(GPS_Data.Velocity_Descend * 100.0);
    uint32_t satellite_number = GPS_Data.Num_Satellites;
    uint32_t vertical_accuracy = gps_push_vertical_accuracy_mm();
    uint32_t horizontal_accuracy = gps_push_horizontal_accuracy_mm();
    uint32_t speed_accuracy = gps_push_speed_accuracy_cms();

    static uint32_t push_log_counter;
    if ((push_log_counter++ % 10) == 0) {
        ESP_LOGI(TAG,
                 "DJI 0x0017 push: lat=%.5f lon=%.5f alt=%.1fm sats=%u "
                 "vN=%.1f vE=%.1f vD=%.1f hAcc=%umm vAcc=%umm",
                 GPS_Data.Latitude, GPS_Data.Longitude, GPS_Data.Altitude, satellite_number,
                 speed_to_north, speed_to_east, speed_to_wnward,
                 (unsigned)horizontal_accuracy, (unsigned)vertical_accuracy);
    }

    gps_data_push_command_frame gps_frame = {
        .year_month_day = year_month_day,
        .hour_minute_second = hour_minute_second,
        .gps_longitude = gps_longitude,
        .gps_latitude = gps_latitude,
        .height = height,
        .speed_to_north = speed_to_north,
        .speed_to_east = speed_to_east,
        .speed_to_wnward = speed_to_wnward,
        .vertical_accuracy = vertical_accuracy,
        .horizontal_accuracy = horizontal_accuracy,
        .speed_accuracy = speed_accuracy,
        .satellite_number = satellite_number
    };

    gps_data_push_response_frame *response = command_logic_push_gps_data(&gps_frame);
    if (response != NULL) {
        free(response);
    }
}

/**
 * @brief 初始化 GPS UART
 *        Initialize GPS UART
 *
 * 配置并初始化 GPS UART，用于接收 GPS 数据。
 * Configure and initialize GPS UART for receiving GPS data.
 */
/**
 * @brief Log a UART chunk safely (binary / wrong baud shows as hex, not blank %s)
 */
static void log_gps_uart_chunk(const uint8_t *data, int len) {
    bool has_nmea = false;
    for (int i = 0; i < len; i++) {
        if (data[i] == '$') {
            has_nmea = true;
            break;
        }
    }

    if (has_nmea) {
        s_gps_last_nmea_us = esp_timer_get_time();
        char line[128];
        int line_len = 0;
        for (int i = 0; i < len && line_len < (int)sizeof(line) - 1; i++) {
            char c = (char)data[i];
            if (c == '\r' || c == '\n') {
                if (line_len > 0) {
                    line[line_len] = '\0';
                    if (line[0] == '$') {
                        ESP_LOGI("RX_TASK_GPS", "NMEA: %s", line);
                    }
                    line_len = 0;
                }
            } else if (line_len < (int)sizeof(line) - 1) {
                line[line_len++] = c;
            }
        }
        if (line_len > 0 && line[0] == '$') {
            line[line_len] = '\0';
            ESP_LOGI("RX_TASK_GPS", "NMEA: %s", line);
        }
    } else {
        int hex_len = len < 16 ? len : 16;
        char hex[16 * 3 + 1];
        int pos = 0;
        for (int i = 0; i < hex_len && pos < (int)sizeof(hex) - 4; i++) {
            pos += snprintf(hex + pos, sizeof(hex) - pos, "%02X ", data[i]);
        }
        ESP_LOGW("RX_TASK_GPS", "RX %d bytes, no $ (check HC-05 baud=115200): %s", len, hex);
    }
}

#if CONFIG_WAVESHARE_ESP32_S3_TOUCH_LCD_128
static void ubx_send(const uint8_t *payload, uint16_t payload_len, uint8_t cls, uint8_t id)
{
    uint8_t frame[64];
    const uint16_t frame_len = (uint16_t)(payload_len + 8);

    if (frame_len > sizeof(frame)) {
        return;
    }

    frame[0] = 0xB5;
    frame[1] = 0x62;
    frame[2] = cls;
    frame[3] = id;
    frame[4] = (uint8_t)(payload_len & 0xFF);
    frame[5] = (uint8_t)(payload_len >> 8);
    memcpy(&frame[6], payload, payload_len);

    uint8_t ck_a = 0;
    uint8_t ck_b = 0;
    for (uint16_t i = 2; i < 6 + payload_len; i++) {
        ck_a = (uint8_t)(ck_a + frame[i]);
        ck_b = (uint8_t)(ck_b + ck_a);
    }
    frame[6 + payload_len] = ck_a;
    frame[7 + payload_len] = ck_b;

    uart_write_bytes(BOARD_GPS_UART_PORT, (const char *)frame, frame_len);
}

static void configure_ublox_neo_m8(void)
{
    static const uint8_t cfg_rate[] = {
        0x64, 0x00, 0x01, 0x00, 0x00, 0x00,
    };
    static const uint8_t cfg_msg_gga[] = { 0xF0, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00 };
    static const uint8_t cfg_msg_rmc[] = { 0xF0, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00 };
    static const uint8_t cfg_msg_gst[] = { 0xF0, 0x07, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00 };

    ubx_send(cfg_rate, sizeof(cfg_rate), 0x06, 0x08);
    ubx_send(cfg_msg_gga, sizeof(cfg_msg_gga), 0x06, 0x01);
    ubx_send(cfg_msg_rmc, sizeof(cfg_msg_rmc), 0x06, 0x01);
    ubx_send(cfg_msg_gst, sizeof(cfg_msg_gst), 0x06, 0x01);
    ESP_LOGI(TAG, "Configured u-blox NEO-M8: %d baud, 10 Hz, GGA+RMC+GST", GPS_UART_BAUD_FINAL);
}
#endif

static void configure_gps_module(void)
{
#if CONFIG_WAVESHARE_ESP32_S3_TOUCH_LCD_128
    configure_ublox_neo_m8();
#else
    const char *gps_command = "$PAIR050,100*22\r\n";
    uart_write_bytes(BOARD_GPS_UART_PORT, gps_command, strlen(gps_command));
#endif
}

static void initUartGps(int baud_rate)
{
    const uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
#if CONFIG_IDF_TARGET_ESP32C6
        .source_clk = LP_UART_SCLK_DEFAULT,
#endif
    };
    uart_driver_install(BOARD_GPS_UART_PORT, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(BOARD_GPS_UART_PORT, &uart_config);
    uart_set_pin(BOARD_GPS_UART_PORT, BOARD_GPS_TXD_PIN, BOARD_GPS_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

static void gps_push_task(void *arg)
{
    (void)arg;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(GPS_PUSH_INTERVAL_MS));
        if (connect_logic_get_state() == PROTOCOL_CONNECTED && is_current_gps_data_valid()) {
            gps_push_data();
        }
    }
}

/**
 * @brief GPS 数据接收任务
 *        GPS data receiving task
 *
 * 从 GPS UART 端口读取数据，解析并处理 NMEA 数据。
 * Read data from GPS UART port, parse and process NMEA data.
 *
 * @param arg 任务参数
 *            Task parameters
 */
static void rx_task_GPS(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK_GPS";
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE + 1);

    while (1) {
        const int rxBytes = uart_read_bytes(BOARD_GPS_UART_PORT, data, RX_BUF_SIZE, 20 / portTICK_PERIOD_MS);
        if (rxBytes > 0) {
            s_gps_rx_bytes_total += (uint32_t)rxBytes;
            // 给看门狗喂狗的机会
            // Give watchdog a chance to reset
            vTaskDelay(pdMS_TO_TICKS(5));

            data[rxBytes] = '\0';

            log_gps_uart_chunk(data, rxBytes);

            // 将读取到的数据存储到全局缓冲区 buff_t 中
            // Store the read data into global buffer buff_t
            memcpy(buff_t, data, rxBytes);
            buff_t[rxBytes] = '\0'; // 确保缓冲区结束
                                    // Ensure buffer termination

            // 解析数据
            // Parse data
            Parse_NMEA_Buffer(buff_t);

            // 给看门狗喂狗的机会
            // Give watchdog a chance to reset
            vTaskDelay(pdMS_TO_TICKS(5));

            if (is_current_gps_data_valid()) {
                print_gps_data();
            }
        }
        // 如果没有数据读取，休眠一小段时间，避免任务占用 CPU
        // If no data is read, sleep for a short time to avoid CPU occupation
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    free(data);
}

uint32_t gps_get_rx_bytes_total(void)
{
    return s_gps_rx_bytes_total;
}

uint32_t gps_get_last_nmea_ms_ago(void)
{
    int64_t last = s_gps_last_nmea_us;
    if (last <= 0) {
        return UINT32_MAX;
    }
    int64_t now = esp_timer_get_time();
    int64_t delta_us = now - last;
    if (delta_us < 0) {
        return UINT32_MAX;
    }
    return (uint32_t)(delta_us / 1000);
}

/**
 * @brief 初始化并启动 GPS 数据接收任务
 *        Initialize and start GPS data receiving task
 *
 * 初始化 GPS UART 和相关任务，以定期接收 GPS 数据。
 * Initialize GPS UART and related tasks to periodically receive GPS data.
 */
void initSendGpsDataToCameraTask(void) {
    init_gps_data();
    initUartGps(GPS_UART_BAUD_PROBE);
    configure_gps_module();
    if (GPS_UART_BAUD_PROBE != GPS_UART_BAUD_FINAL) {
        uart_set_baudrate(BOARD_GPS_UART_PORT, GPS_UART_BAUD_FINAL);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    xTaskCreate(rx_task_GPS, "uart_rx_task_GPS", 1024 * 4, NULL, 0, NULL);
    xTaskCreate(gps_push_task, "gps_push_task", 2048, NULL, 1, NULL);
    ESP_LOGI(TAG, "GPS UART + 10 Hz DJI push task started");
}
