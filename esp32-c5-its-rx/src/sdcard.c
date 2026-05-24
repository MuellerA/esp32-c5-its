#include <stdio.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"

#include "esp32-c5-its.h"

static const char *TAG = "ESP32-C5-ITS sdcard";

#define SPI_HOST    SPI2_HOST
#define MOUNT_POINT "/sdcard"
static sdmmc_card_t *card;
static FILE *logFile ;
static SemaphoreHandle_t logFileMutex;

static TaskHandle_t sdcard_task_handle = NULL;
volatile bool sdcard_active = false;

static void IRAM_ATTR button_isr_handler(void* arg)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(sdcard_task_handle, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

QueueHandle_t sdcard_queue ;

void sdcard_writer_task(void*)
{
  struct LogData log_data;

  while (1)
  {
    if (xQueueReceive(sdcard_queue, &log_data, portMAX_DELAY))
    {
      if (xSemaphoreTake(logFileMutex, pdMS_TO_TICKS(500)) == pdTRUE)
      {
        if (logFile)
        {
          fwrite(&log_data.header, sizeof(log_data.header), 1, logFile) ;
          fwrite(&log_data.body, log_data.header.body_size, 1, logFile) ;
          fflush(logFile) ;
        }
        xSemaphoreGive(logFileMutex) ;
      }
    }
  }
}

esp_err_t mount_sdcard() {
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI_HOST;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SDCARD_CS_GPIO;
    slot_config.host_id = SPI_HOST;

    return esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
}

void unmount_sdcard()
{
  esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
  card = NULL;
}


void sdcard_control_task(void *pvParameters)
{
  while (1)
  {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    if (!sdcard_active)
    {
      if (mount_sdcard() == ESP_OK)
      {
        char filename[64] ;
        if (xSemaphoreTake(logFileMutex, portMAX_DELAY) == pdTRUE)
        {
          sprintf(filename, "%s/log-%lld.dat", MOUNT_POINT, sys_to_gps_time_us(esp_timer_get_time()) / 1000000) ;
          logFile = fopen(filename, "wb");
          xSemaphoreGive(logFileMutex) ;
        }

        if (!logFile)
        {
          ESP_LOGW(TAG, "fopen %s failed", filename);
          unmount_sdcard() ;
        }

        struct LogData data ;
        init_version_log_data(&data) ;
        xQueueSend(sdcard_queue, &data, 0) ;
        init_info_log_data(&data) ;
        xQueueSend(sdcard_queue, &data, 0) ;

        ESP_LOGI(TAG, "sdcard logging start %s", filename) ;
        sdcard_active = true ;
      }
      else
      {
        ESP_LOGW(TAG, "mount %s failed", MOUNT_POINT);
      }
    }
    else
    {
      if (xSemaphoreTake(logFileMutex, portMAX_DELAY) == pdTRUE)
      {
        fsync(fileno(logFile)) ;
        fclose(logFile) ;
        logFile = NULL ;
        xSemaphoreGive(logFileMutex) ;
      }
      sdcard_active = false ;
      ESP_LOGI(TAG, "sdcard logging stop") ;
      unmount_sdcard() ;
    }

    vTaskDelay(pdMS_TO_TICKS(200));
    ulTaskNotifyTake(pdTRUE, 0);
  }
}

void init_sdcard()
{
  xTaskCreate(sdcard_control_task, "sdcard_control", 0x1800, NULL, 10, &sdcard_task_handle);

  gpio_isr_handler_add(SDCARD_BUTTON_GPIO, button_isr_handler, NULL);

  spi_bus_config_t bus_cfg = {
      .mosi_io_num = SDCARD_MOSI_GPIO,
      .miso_io_num = SDCARD_MISO_GPIO,
      .sclk_io_num = SDCARD_CLK_GPIO,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 4000,
  };
  spi_bus_initialize(SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);

  sdcard_queue = xQueueCreate(QUEUE_SIZE, sizeof(struct LogData));
  xTaskCreate(sdcard_writer_task, "sdcard", 4096, NULL, 10, NULL);

  logFileMutex = xSemaphoreCreateMutex() ;
}
