#pragma once

#include <stdint.h>

#include "log-data.h"

#define RGB_LED_GPIO 27

#define GPS_PPS_GPIO 3
#define GPS_TX_GPIO  4
#define GPS_RX_GPIO  5

#define SDCARD_MISO_GPIO 2
#define SDCARD_MOSI_GPIO 7
#define SDCARD_CLK_GPIO  6
#define SDCARD_CS_GPIO  10

#define SDCARD_BUTTON_GPIO 28

#define PRG_VER_MAJ 1
#define PRG_VER_MIN 1
#define PRG_VER_REV 0
#define GIT_REPO "https://github.com/MuellerA/esp32-c5-its"

extern void init_gps();
extern int64_t sys_to_gps_time_us(int64_t now_us);
extern volatile bool gps_time_active ;
extern volatile bool gps_pos_active ;

extern void init_sdcard();
extern volatile bool sdcard_active ;

#define QUEUE_SIZE 10
extern QueueHandle_t usb_queue;

extern volatile bool sdcard_active;
#define QUEUE_SIZE 10
extern QueueHandle_t sdcard_queue;

extern void log_data() ;
extern void info_log_data(char *format, ...) ;
extern void init_info_log_data(struct LogData *data) ;
extern void init_version_log_data(struct LogData *data) ;

