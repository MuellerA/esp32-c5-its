#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "driver/usb_serial_jtag.h"
#include "driver/gpio.h"
#include "led_strip.h"

#include "esp32-c5-its.h"

static const char *TAG = "ESP32-C5-ITS main";

// led

#if defined(RGB)

led_strip_handle_t led_strip;
esp_timer_handle_t led_timer;

static void led_off_timer_cb(void *arg)
{
  uint8_t state = 0;

#if defined(GSP)
  if (gps_time_active)
    state |= 1 ;
  if (gps_pos_active)
    state |= 2 ;
#endif

  switch (state)
  {
  case 1:  led_strip_set_pixel(led_strip, 0, 0, 0, 2); break; // time: blue
  case 2:  led_strip_set_pixel(led_strip, 0, 0, 2, 0); break; // pos:  red
  case 3:  led_strip_set_pixel(led_strip, 0, 0, 2, 0); break; // time&pos: green
  default: led_strip_set_pixel(led_strip, 0, 0, 0, 0); break; // off
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
#if defined(SDCARD)
    if (sdcard_active)
      state |= 2;
#endif

    switch (state)
    {
    case 1:  led_strip_set_pixel(led_strip, 0, 70, 25,  5); break; // usb orange/yellow
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

#endif

#if defined(LED)

esp_timer_handle_t led_timer;

static void led_off_timer_cb(void *arg)
{
  gpio_set_level(LED_GPIO, 1);
}

static TaskHandle_t led_flash_task = NULL;
void led_flash(void *pvParameters)
{
  while (1)
  {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    gpio_set_level(LED_GPIO, 0);

    esp_timer_stop(led_timer);
    esp_timer_start_once(led_timer, 50000); // us
  }
}

static void led_init()
{
  gpio_reset_pin(LED_GPIO);
  gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

  led_off_timer_cb(NULL);

  const esp_timer_create_args_t timer_args = {
      .callback = &led_off_timer_cb,
      .name = "led_off"
    };
  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &led_timer));
}

#endif

// usb

QueueHandle_t usb_queue;

void usb_transmitter_task(void *)
{
  struct LogData data;
  static const uint8_t magic[4] = {0xaa, 0x55, 0x55, 0xaa};

  usb_serial_jtag_driver_config_t usb_config = {
      .tx_buffer_size = 2048,
      .rx_buffer_size = 256
  };
  usb_serial_jtag_driver_install(&usb_config);

  init_version_log_data(&data) ;
  xQueueSend(usb_queue, &data, 0) ;
  init_info_log_data(&data) ;
  xQueueSend(usb_queue, &data, 0) ;

  while (1)
  {
    if (xQueueReceive(usb_queue, &data, portMAX_DELAY))
    {
      if (usb_serial_jtag_is_connected())
      {
        usb_serial_jtag_write_bytes(magic, sizeof(magic), portMAX_DELAY);
        usb_serial_jtag_write_bytes(&data.header, sizeof(data.header), portMAX_DELAY);
        usb_serial_jtag_write_bytes(&data.body, data.header.body_size, portMAX_DELAY);
      }
    }
  }
}

// its

void its_log_data(void *buf, wifi_promiscuous_pkt_type_t type)
{
  static const uint8_t bc_addr[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

  uint64_t current_time = esp_timer_get_time();

  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;

  if (!memcmp(pkt->payload + 4, bc_addr, sizeof(bc_addr)))
  {
    if (pkt->rx_ctrl.sig_len <= MAX_PKT_SIZE)
    {
      struct LogData data;
      data.header.pkt_type = LOG_DATA_TYPE_ITS;
      data.header.reserved = 0;
      data.header.body_size = pkt->rx_ctrl.sig_len;
      data.header.timestamp_us = sys_to_gps_time_us(current_time);
      memcpy(data.body.its.payload, pkt->payload, data.header.body_size);
      log_data(&data) ;
    }
  }
}

void init_version_log_data(struct LogData *data)
{
  int64_t current_time = esp_timer_get_time();

  data->header.pkt_type = LOG_DATA_TYPE_VERSION;
  data->header.reserved = 0;
  data->header.body_size = sizeof(data->body.version) ;
  data->header.timestamp_us = sys_to_gps_time_us(current_time) ;
  data->body.version.logVersion = LOG_DATA_VERSION ;
  data->body.version.prgVerMaj = PRG_VER_MAJ ;
  data->body.version.prgVerMin = PRG_VER_MIN ;
  data->body.version.prgVerRev = PRG_VER_REV ;
}

static char info_log_data_buff[1024] ;
static char *info_log_data_insert = info_log_data_buff ;
void info_log_data(char *format, ...)
{
  va_list args;
  va_start(args, format) ;

  info_log_data_insert += vsnprintf(info_log_data_insert,
                                    sizeof(info_log_data_buff) - (info_log_data_insert - info_log_data_buff),
                                    format, args) ;
}

void init_info_log_data(struct LogData *data)
{
  int64_t current_time = esp_timer_get_time();

  data->header.pkt_type = LOG_DATA_TYPE_INFO;
  data->header.reserved = 0;
  data->header.body_size = strlen(info_log_data_buff) ;
  data->header.timestamp_us = sys_to_gps_time_us(current_time) ;
  strcpy(data->body.info.text, info_log_data_buff) ;
}

void log_data(struct LogData *data)
{
  if (usb_serial_jtag_is_connected() &&
      (xQueueSend(usb_queue, data, 0) != pdTRUE))
  {
    ESP_LOGW(TAG, "usb queue full");
  }

#if defined(SDCARD)
  if (sdcard_active &&
      (data->header.pkt_type != LOG_DATA_TYPE_TIME) &&
      (xQueueSend(sdcard_queue, data, 0) != pdTRUE))
  {
    ESP_LOGW(TAG, "sdcard queue full");
  }
#endif

#if defined(RGB) || defined(LED)
  xTaskNotifyGive(led_flash_task) ;
#endif
}

extern void phy_change_channel(int, int, int, int);
extern void phy_11p_set(int, int);

void gpio_init()
{
#if defined(GPS)
  gpio_config_t io_conf_gps = {
    .pin_bit_mask = (1ULL << GPS_PPS_GPIO),
    .intr_type = GPIO_INTR_POSEDGE,
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_ENABLE,
  };
  gpio_config(&io_conf_gps);
#endif

#if defined(SDCARD)
  gpio_config_t io_conf_sdcard = {
    .pin_bit_mask = (1ULL << SDCARD_BUTTON_GPIO),
    .intr_type = GPIO_INTR_NEGEDGE,
    .mode = GPIO_MODE_INPUT,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .pull_up_en = GPIO_PULLUP_ENABLE,
  };
  gpio_config(&io_conf_sdcard);
#endif

  ESP_ERROR_CHECK(gpio_install_isr_service(0));
}

void app_main()
{
  {
    info_log_data("esp32-c5-its version: %d.%d.%d\n", PRG_VER_MAJ, PRG_VER_MIN, PRG_VER_REV) ;
    char options[80] = {0} ;
#if defined(GPS)
    strcat(options, " GPS") ;
#endif
#if defined(SDCARD)
    strcat(options, " SDCARD") ;
#endif
#if defined(RGB)
    strcat(options, " RGB") ;
#endif
#if defined(LED)
    strcat(options, " LED") ;
#endif
    info_log_data("esp32-c5-its options:%s\n", options) ;
    info_log_data("log file version: %d\n", LOG_DATA_VERSION) ;
    info_log_data("git repo: %s\n", GIT_REPO) ;
    {
      esp_chip_info_t chip_info;
      esp_chip_info(&chip_info) ;
      info_log_data("esp model: %s\n", CONFIG_IDF_TARGET) ;
      info_log_data("esp cores: %d\n", chip_info.cores) ;
      info_log_data("esp chip rev: %d.%d\n", chip_info.revision / 100, chip_info.revision % 100);
    }
    {
      uint32_t flash_size;
      esp_flash_get_size(NULL, &flash_size) ;
      info_log_data("esp flash: %lu MB\n", flash_size / (1024 * 1024));
    }
    {
      uint8_t mac[6];
      esp_read_mac(mac, ESP_MAC_BASE) ;
      info_log_data("esp mac: %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    info_log_data("esp idf version: %s\n", esp_get_idf_version());

    ESP_LOGI(TAG, "%s", info_log_data_buff);
  }

  gpio_init() ;

  nvs_flash_init();
  esp_netif_init();

#if defined(RGB) || defined(LED)
  led_init();
#endif

  init_gps();

#if defined(SDCARD)
  init_sdcard();
#endif

  esp_event_loop_create_default();

#if defined(RGB) || defined(LED)
  xTaskCreate(led_flash, "led_flash", 4096, NULL, 10, &led_flash_task);
#endif

  usb_queue = xQueueCreate(QUEUE_SIZE, sizeof(struct LogData));
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
  esp_wifi_set_promiscuous_rx_cb(its_log_data);

  ESP_LOGI(TAG, "ready");
}