#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "driver/usb_serial_jtag.h"
#include "driver/gpio.h"
#include "led_strip.h"

#include "esp32-c5-its.h"

static const char *TAG = "ESP32-C5-ITS main";

// led

led_strip_handle_t led_strip;
esp_timer_handle_t led_timer;

static void led_off_timer_cb(void *arg)
{
  uint8_t state = 0;

  if (gps_time_active)
    state = 1 ;
  if (gps_pos_active)
    state = 2 ;

  switch (state)
  {
  case 1:  led_strip_set_pixel(led_strip, 0, 0, 0, 2); break; // green
  case 2:  led_strip_set_pixel(led_strip, 0, 0, 2, 0); break; // blue
  default: led_strip_set_pixel(led_strip, 0, 1, 0, 0); break; // red
  }
  led_strip_refresh(led_strip);
}

static TaskHandle_t led_flash_task = NULL;
void led_flash(void *pvParameters)
{
  while (1)
  {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    uint8_t state = 0;

    if (usb_serial_jtag_is_connected())
      state |= 1;
    if (sdcard_active)
      state |= 2;

    switch (state)
    {
    case 1:  led_strip_set_pixel(led_strip, 0, 25, 50,  0); break; // usb yellow
    case 2:  led_strip_set_pixel(led_strip, 0,  0,  0, 99); break; // sdcard blue
    case 3:  led_strip_set_pixel(led_strip, 0,  0, 99,  0); break; // usb & sdcard green
    default: led_strip_set_pixel(led_strip, 0, 50,  0,  0); break; // none red
    }
    led_strip_refresh(led_strip);

    esp_timer_stop(led_timer);
    esp_timer_start_once(led_timer, 50000); // us
  }
}

static void led_init()
{
  led_strip_config_t strip_config =
      {
          .strip_gpio_num = RGB_LED_GPIO,
          .max_leds = 1,
      };
  led_strip_rmt_config_t rmt_config = {
      .resolution_hz = 10 * 1000 * 1000, // 10MHz
  };
  ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
  led_off_timer_cb(NULL);

  const esp_timer_create_args_t timer_args = {
      .callback = &led_off_timer_cb,
      .name = "led_off"
    };
  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &led_timer));
}

// usb

QueueHandle_t usb_queue;

void usb_transmitter_task(void *)
{
  log_data_t data;
  static const uint8_t magic[4] = {0xaa, 0x55, 0x55, 0xaa};

  usb_serial_jtag_driver_config_t usb_config = {
      .tx_buffer_size = 2048,
      .rx_buffer_size = 256
  };
  usb_serial_jtag_driver_install(&usb_config);

  while (1)
  {
    if (xQueueReceive(usb_queue, &data, portMAX_DELAY))
    {
      if (usb_serial_jtag_is_connected())
      {
        usb_serial_jtag_write_bytes(magic, sizeof(magic), portMAX_DELAY);
        usb_serial_jtag_write_bytes(&data.type, 1, portMAX_DELAY);
        usb_serial_jtag_write_bytes(&data.timestamp_us, sizeof(int64_t), portMAX_DELAY);
        usb_serial_jtag_write_bytes(&data.size, sizeof(uint16_t), portMAX_DELAY);

        switch (data.type)
        {
        case LOG_DATA_TYPE_ITS:
          usb_serial_jtag_write_bytes(data.its.payload, data.size, portMAX_DELAY);
          break;
        case LOG_DATA_TYPE_GPS:
          usb_serial_jtag_write_bytes(&data.gps.quality, sizeof(int8_t), portMAX_DELAY);
          usb_serial_jtag_write_bytes(&data.gps.latitude, sizeof(int32_t), portMAX_DELAY);
          usb_serial_jtag_write_bytes(&data.gps.longitude, sizeof(int32_t), portMAX_DELAY);
          usb_serial_jtag_write_bytes(&data.gps.altitude, sizeof(int32_t), portMAX_DELAY);
          break;
        }
      }
    }
  }
}

// wifi

void rx_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
  static const uint8_t bc_addr[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

  uint64_t current_time = esp_timer_get_time();

  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;

  if (!memcmp(pkt->payload + 4, bc_addr, sizeof(bc_addr)))
  {
    if (pkt->rx_ctrl.sig_len <= MAX_PKT_SIZE)
    {
      log_data_t data;
      data.type = LOG_DATA_TYPE_ITS;
      data.timestamp_us = sys_to_gps_time_us(current_time);
      data.size = pkt->rx_ctrl.sig_len;
      memcpy(data.its.payload, pkt->payload, data.size);
      log_data(&data) ;
    }
  }
}

void log_data(log_data_t *data)
{
  if (usb_serial_jtag_is_connected && xQueueSend(usb_queue, data, 0) != pdTRUE)
  {
    ESP_LOGW(TAG, "usb queue full");
  }
  if (sdcard_active && xQueueSend(sdcard_queue, data, 0) != pdTRUE)
  {
    ESP_LOGW(TAG, "sdcard queue full");
  }
  xTaskNotifyGive(led_flash_task) ;
}

extern void phy_change_channel(int, int, int, int);
extern void phy_11p_set(int, int);

void app_main()
{
  gpio_install_isr_service(0);

  led_init();

  nvs_flash_init();
  esp_netif_init();

  init_gps();
  init_sdcard();

  esp_event_loop_create_default();

  xTaskCreate(led_flash, "led_flash", 4096, NULL, 10, &led_flash_task);

  usb_queue = xQueueCreate(QUEUE_SIZE, sizeof(log_data_t));
  xTaskCreate(usb_transmitter_task, "usb", 4096, NULL, 10, NULL);

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_NULL);
  esp_wifi_start();
  esp_wifi_set_band(WIFI_BAND_5G);
  esp_wifi_set_channel(140, WIFI_SECOND_CHAN_NONE);
  if (true)
  {
    phy_11p_set(1, 0);
    phy_change_channel(5900, 1, 0, 0);
  }

  wifi_promiscuous_filter_t filter =
      {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_DATA | WIFI_PROMIS_FILTER_MASK_MGMT
        //.filter_mask = WIFI_PROMIS_FILTER_MASK_ALL
      };
  esp_wifi_set_promiscuous_filter(&filter);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(rx_cb);

  ESP_LOGI(TAG, "ready");
}