#if defined(GPS)

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
#include "gps.h"

static const char *TAG = "ESP32-C5-ITS gps";

#define UART_PORT_NUM UART_NUM_1

volatile bool gps_time_active ;
volatile bool gps_pos_active ;


#define GPS_BUF_SIZE 1024

typedef enum
{
  GPS_RECV_TIME_PPS = 1,
} gps_recv_time_t ;
volatile uint8_t gps_recv_time = 0 ;

volatile struct Gps gps_data ;

volatile int64_t sys_tick_at_last_pps = 0 ;

volatile int64_t offset_utc_to_sys_tick = 0 ;

static void IRAM_ATTR gps_pps_isr_handler(void* arg)
{
  sys_tick_at_last_pps = esp_timer_get_time();
  gps_recv_time = GPS_RECV_TIME_PPS ;
}

int64_t sys_to_gps_time_us(int64_t sys_us)
{
  //ESP_LOGI(TAG, "%10" PRId64 " %10" PRId64 " %10" PRId64, sys_us, offset_utc_to_sys_tick, sys_tick_at_last_pps) ;
  //ESP_LOGI(TAG, "%10" PRId64, (sys_us + offset_utc_to_sys_tick) / 1000000) ;
  return sys_us + offset_utc_to_sys_tick ;
}

esp_timer_handle_t gps_log_data_timer;
void gps_log_data(void *pvParameters)
{
  struct LogData data;

  int64_t current_time = esp_timer_get_time();

  if (gps_pos_active)
  {
    data.header.pkt_type = LOG_DATA_TYPE_GPS;
    data.header.reserved = 0;
    data.header.body_size = sizeof(data.body.gps) ;
    data.header.timestamp_us = sys_to_gps_time_us(current_time);

    data.body.gps = gps_data ;
  }
  else
  {
    data.header.pkt_type = LOG_DATA_TYPE_TIME;
    data.header.reserved = 0;
    data.header.body_size = 0;
    data.header.timestamp_us = sys_to_gps_time_us(current_time);
  }

  log_data(&data) ;
}

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

void init_gps()
{
  init_gps_uart();

  bool ubx = gps_ubx_init() ;

  ESP_ERROR_CHECK(gpio_isr_handler_add(GPS_PPS_GPIO, gps_pps_isr_handler, NULL));

  const esp_timer_create_args_t gps_log_data_timer_args = {
      .callback = &gps_log_data,
      .name = "gps_log_data"
  };
  ESP_ERROR_CHECK(esp_timer_create(&gps_log_data_timer_args, &gps_log_data_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(gps_log_data_timer, 1000000));

  if (ubx)
    xTaskCreate(gps_ubx_read_task, "gps_ubx_read_task", 4096, NULL, 5, NULL);
  else
    xTaskCreate(gps_nmea_read_task, "gps_read_task", 4096, NULL, 5, NULL);
}

#endif