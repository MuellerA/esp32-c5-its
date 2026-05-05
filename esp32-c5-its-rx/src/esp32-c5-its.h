#include <stdint.h>

#pragma once

#define RGB_LED_GPIO 27

#define GPS_PPS_GPIO 3
#define GPS_TX_GPIO  4
#define GPS_RX_GPIO  5

#define SDCARD_MISO_GPIO 2
#define SDCARD_MOSI_GPIO 7
#define SDCARD_CLK_GPIO  6
#define SDCARD_CS_GPIO  10

#define SDCARD_BUTTON_GPIO 28

extern void init_gps();
extern int64_t sys_to_gps_time_us(int64_t now_us);
extern volatile bool gps_time_active ;
extern volatile bool gps_pos_active ;

extern void init_sdcard();
extern volatile bool sdcard_active ;


typedef enum
{
  LOG_DATA_TYPE_ITS = 1,
  LOG_DATA_TYPE_GPS = 2,
} log_data_type_t;

#define MAX_PKT_SIZE 2000
#pragma pack(push, 1)
typedef struct
{
  uint8_t type;
  int64_t timestamp_us; // UTC time in µs
  uint16_t size;

  union
  {
    struct
    {
      uint8_t payload[MAX_PKT_SIZE];
    } its;
    struct
    {
      uint8_t quality;
      int32_t latitude;
      int32_t longitude;
      int32_t altitude;
    } gps;
  };
} log_data_t;
#pragma pack(pop)

#define QUEUE_SIZE 10
extern QueueHandle_t usb_queue;

extern volatile bool sdcard_active;
#define QUEUE_SIZE 10
extern QueueHandle_t sdcard_queue;

extern void log_data() ;