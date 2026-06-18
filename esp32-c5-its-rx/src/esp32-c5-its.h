#pragma once

#include <stdint.h>

#include "log-data.h"

#if defined(RGB)
# define RGB_LED_GPIO 27
#endif

#if defined(LED)
# define LED_GPIO 27
#endif

#if defined(GPS)
# define GPS_PPS_GPIO 3
# define GPS_TX_GPIO  4
# define GPS_RX_GPIO  5
#endif

#if defined(SDCARD)
# define SDCARD_MISO_GPIO 2
# define SDCARD_MOSI_GPIO 7
# define SDCARD_CLK_GPIO  6
# define SDCARD_CS_GPIO  10

# define SDCARD_BUTTON_GPIO 28
#endif

#define PRG_VER_MAJ 1
#define PRG_VER_MIN 2
#define PRG_VER_REV 0
#define GIT_REPO "https://github.com/MuellerA/esp32-c5-its"

#if defined(GPS)
extern volatile bool gps_time_active ;
extern volatile bool gps_pos_active ;
#endif
extern void init_gps();
extern int64_t sys_to_gps_time_us(int64_t now_us);

#if defined(SDCARD)
extern void init_sdcard();
extern volatile bool sdcard_active ;
#endif

#define QUEUE_SIZE 10
extern QueueHandle_t usb_queue;

#if defined(SDCARD)
extern volatile bool sdcard_active;
extern QueueHandle_t sdcard_queue;
#endif

extern void log_data() ;
extern void info_log_data(char *format, ...) ;
extern void init_info_log_data(struct LogData *data) ;
extern void init_version_log_data(struct LogData *data) ;

