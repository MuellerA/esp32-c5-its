#include <stdint.h>

#pragma once

#define RGB_LED_GPIO 27 // Typischer GPIO für interne RGB-LED beim C5

#define GPS_PPS_GPIO 3
#define GPS_TX_GPIO  4
#define GPS_RX_GPIO  5

#define SDCARD_MISO_GPIO 2
#define SDCARD_MOSI_GPIO 7
#define SDCARD_CLK_GPIO  6
#define SDCARD_CS_GPIO  10

extern const char *TAG ;


extern void init_gps() ;
extern int64_t sys_to_gps_time_us(int64_t now_us) ;


typedef enum {
    PKT_TYPE_ITS = 1,
    PKT_TYPE_GPS = 2,
} pkt_type_t;

#define MAX_PKT_SIZE 2000
#pragma pack(push, 1)
typedef struct
{
  uint8_t pkt_type ;
  int64_t timestamp_us; // UTC Zeit in µs
  uint16_t length;

  union
  {
    struct
    {
      uint8_t payload[MAX_PKT_SIZE];
    } its ;
    struct
    {
      uint8_t quality;
      int32_t latitude;
      int32_t longitude;
      int32_t altitude;
    } gps ;
  };
} usb_data_t;
#pragma pack(pop)

#define QUEUE_SIZE 10
extern QueueHandle_t usb_queue;

