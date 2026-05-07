#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#include "esp32-c5-its.h"

static const char *TAG = "ESP32-C5-ITS gps";

#define UART_PORT_NUM UART_NUM_1


volatile bool gps_time_active ;
volatile bool gps_pos_active ;


#define GPS_BUF_SIZE 1024

void init_gps_uart() {
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    
    uart_driver_install(UART_PORT_NUM, GPS_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_set_pin(UART_PORT_NUM, GPS_TX_GPIO, GPS_RX_GPIO, -1, -1);
}

typedef enum
{
  GPS_RECV_TIME_PPS = 1,
  GPS_RECV_TIME_GGA = 2,
  GPS_RECV_TIME_RMC = 4,
  GPS_RECV_TIME_ALL = 7,
} gps_recv_time_t ;
static volatile uint8_t gps_recv_time = 0 ;

static volatile uint8_t gps_fix = 0 ;
static volatile int32_t gps_lat = 0 ;
static volatile int32_t gps_lon = 0 ;
static volatile int32_t gps_alt = 0 ;

static volatile int64_t sys_tick_at_last_pps = 0 ;
static volatile uint32_t time_at_last_gga = 0 ;
static volatile uint32_t date_at_last_rmc = 0 ;

static volatile int64_t offset_utc_to_sys_tick = 0 ;

void calc_offset()
{
  static bool first = 1 ;
  struct tm t = {0};

  t.tm_year   = 2000 + (date_at_last_rmc % 100) - 1900;
  t.tm_mon    = (date_at_last_rmc % 10000) / 100 - 1;
  t.tm_mday   = date_at_last_rmc / 10000 ;
  t.tm_hour   = time_at_last_gga / 10000 ;
  t.tm_min    = (time_at_last_gga % 10000) / 100 ;
  t.tm_sec    = time_at_last_gga % 100 ;

  int64_t utc_us = mktime(&t) * 1000000;

  offset_utc_to_sys_tick = utc_us - sys_tick_at_last_pps ;
  gps_recv_time = 0 ;
  gps_time_active = 1 ;

  if (!first)
    return ;

  first = 0 ;
  struct timeval now = { .tv_sec = utc_us / 1000000, .tv_usec = utc_us % 1000000 };
  settimeofday(&now, NULL);
}

void gps_read_task(void *pvParameters)
{
  uint8_t data;

  char line_buffer[128];
  int line_idx = 0;

  while (1)
  {
    int len = uart_read_bytes(UART_PORT_NUM, &data, 1, pdMS_TO_TICKS(100));
    if (len > 0)
    {
      char c = data;

      if (c == '$')
      {
        line_idx = 0 ;
        line_buffer[line_idx++] = c;
      }
      else if (c == '\n' || c == '\r')
      {
        line_buffer[line_idx] = 0;

        if (strncmp(line_buffer, "$GPGGA", 6) == 0)
        {
          // Format: $GPGGA,utc,lat,lat dir,lon,lon dir,quality,# sats,hdop,alt,a-units,undulation,u-units,age,stn ID,*xx
          //               0   1   2       3   4       5       6      7    8   9       A          B       C   D      E
          // $GPGGA,202530.00,5109.0262,N,11401.8407,W,5,40,0.5,1097.36,M,-17.00,M,18,TSTR*61
          size_t nComma = 0 ;
          size_t comma[20] ;
          for (size_t i = 6 ; (i < line_idx) && (nComma < sizeof(comma)) ; ++i)
          {
            if (line_buffer[i] == ',')
            {
              comma[nComma++] = i ;
              line_buffer[i] = 0 ;
            }
          }

          if ((nComma > 1) && ((comma[0] + 1 != comma[1])))
          {
            float utc_time = atof(line_buffer + comma[0]+1) ;
            time_at_last_gga = (uint32_t) utc_time ;

            gps_recv_time |= GPS_RECV_TIME_GGA ;
            if (gps_recv_time == GPS_RECV_TIME_ALL)
              calc_offset() ;
          }
          else
          {
            gps_time_active = 0 ;
          }
          
          gps_fix = 0 ;
          gps_lat = 0 ;
          gps_lon = 0 ;
          gps_alt = 0 ;

          if (nComma > 9)
          {
            if (comma[5] + 1 != comma[6])
            {
              gps_fix = atoi(line_buffer + comma[5] + 1) ;
            }

            if (gps_fix > 0)
            {
              if (comma[1] + 1 != comma[2])
              {
                float lat = atof(line_buffer + comma[1] + 1) ;
                gps_lat = (int32_t)((trunc(lat / 100.0) + (fmod(lat, 100.0) / 60.0)) * 1e7) ;
              }

              if (comma[2] + 1 != comma[3])
              {
                char dir = line_buffer[comma[2]+1] ;
                if (dir == 'S')
                  gps_lat = -gps_lat ;
              }

              if (comma[3] + 1 != comma[4])
              {
                float lon = atof(line_buffer + comma[3] + 1) ;
                gps_lon = (int32_t)((trunc(lon / 100) + (fmod(lon, 100.0) / 60.0)) * 1e7);
              }

              if (comma[4] + 1 != comma[5])
              {
                char dir = line_buffer[comma[4]+1] ;
                if (dir == 'W')
                  gps_lon = - gps_lon ;
              }

              if (comma[8] + 1 != comma[9])
              {
                float alt = atof(line_buffer + comma[8] + 1) ;
                gps_alt = (int32_t)(alt * 10);
              }

              gps_pos_active = 1 ;
            }
            else
            {
              gps_pos_active = 0 ;
            }
          }
        }
        else if (strncmp(line_buffer, "$GPRMC", 6) == 0 || strncmp(line_buffer, "$GNRMC", 6) == 0)
        {
          // Format: $GPRMC,time,status,lat,N,lon,E,spd,cog,date,mv,mvE,mode*cs
          //               0    1      2   3 4   5 6   7   8    9  A   B
          // $GNRMC,204520.00,A,5109.0262239,N,11401.8407338,W,0.004,102.3,130522,0.0,E,D*3B
          size_t nComma = 0 ;
          size_t comma[20] ;
          for (size_t i = 6 ; (i < line_idx) && (nComma < sizeof(comma)) ; ++i)
          {
            if (line_buffer[i] == ',')
            {
              comma[nComma++] = i ;
              line_buffer[i] = 0 ;
            }
          }
          if ((nComma > 9) && ((comma[8] + 6 + 1) == comma[9]))
          {
            date_at_last_rmc = atoi(line_buffer + comma[8]+1) ;

            gps_recv_time |= GPS_RECV_TIME_RMC ;
            if (gps_recv_time == GPS_RECV_TIME_ALL)
              calc_offset() ;
          }
          else
          {
            gps_time_active = 0 ;
          }
        }
        line_idx = 0;
      }
      else if (line_idx < sizeof(line_buffer) - 1)
      {
        line_buffer[line_idx++] = c;
      }
    }
  }
}

static void IRAM_ATTR gps_pps_isr_handler(void* arg)
{
  sys_tick_at_last_pps = esp_timer_get_time();
  gps_recv_time = GPS_RECV_TIME_PPS ;
}

int64_t sys_to_gps_time_us(int64_t sys_us)
{
  return sys_us + offset_utc_to_sys_tick ;
}

esp_timer_handle_t gps_log_data_timer;
void gps_log_data(void *pvParameters)
{
  log_data_t data;

  int64_t current_time = esp_timer_get_time();

  data.type = LOG_DATA_TYPE_GPS;
  data.timestamp_us = sys_to_gps_time_us(current_time);
  data.size = 13 ;

  data.gps.quality = gps_fix ;
  data.gps.latitude = gps_lat ;
  data.gps.longitude = gps_lon ;
  data.gps.altitude = gps_alt ;

  log_data(&data) ;
}

void init_gps()
{
  init_gps_uart();

  gpio_config_t io_conf = {
      .intr_type = GPIO_INTR_POSEDGE,
      .pin_bit_mask = (1ULL << GPS_PPS_GPIO),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_ENABLE
  };
  gpio_config(&io_conf);

  gpio_isr_handler_add(GPS_PPS_GPIO, gps_pps_isr_handler, NULL);

  const esp_timer_create_args_t gps_log_data_timer_args = {
      .callback = &gps_log_data,
      .name = "mein_sekunden_timer"
  };
  esp_timer_create(&gps_log_data_timer_args, &gps_log_data_timer);
  esp_timer_start_periodic(gps_log_data_timer, 1000000);

  xTaskCreate(gps_read_task, "gps_read_task", 4096, NULL, 5, NULL);
}