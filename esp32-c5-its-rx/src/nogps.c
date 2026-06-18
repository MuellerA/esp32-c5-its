#if !defined(GPS)

#include "freertos/FreeRTOS.h"
#include "esp_timer.h"

#include "esp32-c5-its.h"

int64_t sys_to_gps_time_us(int64_t sys_us)
{
  return sys_us ;
}

esp_timer_handle_t gps_log_data_timer;
void gps_log_data(void *pvParameters)
{
  struct LogData data;

  int64_t current_time = esp_timer_get_time();

  data.header.pkt_type = LOG_DATA_TYPE_TIME;
  data.header.reserved = 0;
  data.header.body_size = 0;
  data.header.timestamp_us = current_time;

  log_data(&data) ;
}

void init_gps()
{
  const esp_timer_create_args_t gps_log_data_timer_args = {
      .callback = &gps_log_data,
      .name = "gps_log_data"
  };
  ESP_ERROR_CHECK(esp_timer_create(&gps_log_data_timer_args, &gps_log_data_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(gps_log_data_timer, 1000000));
}

#endif
