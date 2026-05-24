#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#include "esp32-c5-its.h"
#include "gps.h"

static const char *TAG = "ESP32-C5-ITS gps-ubx";

#define UART_PORT_NUM UART_NUM_1


#pragma pack(push,1)

typedef struct
{
  uint8_t portID ;
  uint8_t reserved1 ;
  uint16_t txReady ;
  uint32_t mode ;
  uint32_t baudRate ;
  uint16_t inProtoMask ;
  uint16_t outProtoMask ;
  uint16_t flags ;
  uint16_t reserved2 ;
} frame_cfg_prt_t ;

typedef struct
{
  uint32_t iTOW ; // ms
  int32_t  lon ;  // deg * 1e7
  int32_t  lat ;  // deg * 1e7
  int32_t  alt ;  // mm
  int32_t  hMSL ; // mm
  uint32_t hAcc ; // mm
  uint32_t vAcc ; // mm
} frame_nav_posllh_t ;

typedef struct
{
  uint32_t iTOW ;   // ms
  uint8_t  gpsFix ; // 0 no fix, 1 dead, 2 2d, 3 3d, 4 gps + dead, 5 time
  uint8_t  flags ;
  uint8_t  fixStat ;
  uint8_t  flags2 ;
  uint32_t ttff ;
  uint32_t msss ;
} frame_nav_status_t ;

typedef struct
{
  uint32_t iTOW ; // ms
  int32_t  velN ; // cm/s
  int32_t  velE ; // cm/s
  int32_t  velD ; // cm/s
  uint32_t speed ; // cm/s
  uint32_t gSpeed ; // cm/s
  int32_t  heading ; // deg 1e-5
  uint32_t sAcc ; // cm/s
  uint32_t cAcc ; // deg 1e-5
} frame_nav_velned_t ;

typedef struct
{
  uint32_t iTOW ;  // ms
  uint32_t tAcc ;  // ns
  int32_t  nano ;  // ns
  uint16_t year ;  // y
  uint8_t  month ; // m
  uint8_t  day ;   // d
  uint8_t  hour ;  // d
  uint8_t  min ;   // m
  uint8_t  sec ;   // s
  uint8_t  valid ;
} frame_nav_timeutc_t ;

typedef struct
{
  uint8_t clsId ;
  uint8_t msgId ;
  uint16_t len ;
  union
  {
    uint8_t payload[512] ;
    frame_cfg_prt_t cfg_prt ;
    frame_nav_posllh_t nav_posllh ;
    frame_nav_status_t nav_status ;
    frame_nav_velned_t nav_velned ;
    frame_nav_timeutc_t nav_timeutc ;
  };

} gps_ubx_frame_t ;

#pragma pack(pop)

typedef enum
{
  GPS_RECV_TIME_PPS    =  1,
  GPS_RECV_NAV_TIMEUTC =  2,
  GPS_RECV_TIME        =  3,
} gps_recv_time_t ;

static frame_nav_posllh_t last_nav_posllh ;
static frame_nav_status_t last_nav_status ;
static frame_nav_velned_t last_nav_velned ;

bool uart_get_byte(uint8_t *b)
{
  int len = uart_read_bytes(UART_PORT_NUM, b, 1, pdMS_TO_TICKS(500));
  if (len != 1)
  {
    return false ;
  }

  return true ;
}

bool gps_ubx_collect(gps_ubx_frame_t *rx)
{
  uint8_t b ;

  //ESP_LOGI(TAG, "collect") ;

  if (!uart_get_byte(&rx->clsId))
    return false ;

  if (!uart_get_byte(&rx->msgId))
    return false ;

  if (!uart_get_byte(&b))
    return false ;
  rx->len = b ;

  if (!uart_get_byte(&b))
    return false ;
  rx->len += b*256 ;

  if (rx->len > sizeof(rx->payload))
  {
    ESP_LOGI(TAG, "collect %02x %02x too big: %d", rx->clsId, rx->msgId, rx->len) ;
    return false ;
  }

  for (uint16_t i = 0 ; i < rx->len ; ++i)
  {
    if (!uart_get_byte(rx->payload + i))
      return false ;
  }

  uint8_t chkArx = 0 ;
  if (!uart_get_byte(&chkArx))
    return false ;

  uint8_t chkBrx = 0 ;
  if (!uart_get_byte(&chkBrx))
    return false ;

  uint8_t chkAcalc = 0 ;
  uint8_t chkBcalc = 0 ;

  for (uint16_t i = 0 ; i < (rx->len + 4) ; ++i)
  {
    chkAcalc += ((uint8_t*)(rx))[i] ;
    chkBcalc += chkAcalc ;
  }

  if ((chkAcalc != chkArx) || (chkBcalc != chkBrx))
  {
    ESP_LOGI(TAG, "collect %02x %02x checksum failed", rx->clsId, rx->msgId) ;
    return false ;
  }

  return true ;
}

bool gps_ubx_rx(gps_ubx_frame_t *rx)
{
  uint16_t magic = 0 ;

  while (true)
  {
    uint8_t b ;
    if (!uart_get_byte(&b))
      return false ;

    magic >>= 8 ;
    magic |= b<<8 ;
    if (b == '$')
      ESP_LOGI(TAG, "uart read '$'") ;

    if (magic == 0x62b5)
      return gps_ubx_collect(rx) ;
  }
}

bool gps_ubx_tx(gps_ubx_frame_t *tx)
{
  ESP_LOGI(TAG, "tx %02x %02x", tx->clsId, tx->msgId);

  uint16_t magic = 0x62b5 ;
  if (uart_write_bytes(UART_PORT_NUM, &magic, 2) != 2)
  {
    ESP_LOGI(TAG, "uart magic write failed");

    return false;
  }
  if (uart_write_bytes(UART_PORT_NUM, tx, tx->len + 4) != tx->len + 4)
  {
    ESP_LOGI(TAG, "uart payload write failed");
    return false;
  }

  uint8_t chk[2] = { 0, 0 } ;

  for (uint16_t i = 0 ; i < (tx->len + 4) ; ++i)
  {
    chk[0] = chk[0] + ((uint8_t*)(tx))[i] ;
    chk[1] = chk[1] + chk[0] ;
  }

  if (uart_write_bytes(UART_PORT_NUM, &chk, 2) != 2)
  {
    ESP_LOGI(TAG, "uart chk write failed");
    return false;
  }

  return true;
}

bool gps_ubx_cmd_poll(gps_ubx_frame_t *tx, gps_ubx_frame_t *rx)
{
  if (!gps_ubx_tx(tx))
    return false ;

  gps_ubx_frame_t rxTry ;
  bool found = false ;
  for (uint8_t try = 0 ; try < 32 ; ++try)
  {
    if (!gps_ubx_rx(&rxTry))
      return false ;

    if ((tx->clsId == rxTry.clsId) && (tx->msgId == rxTry.msgId))
    {
      found = true ;
      ESP_LOGI(TAG, "cmd poll found: %02x %02x %d", tx->clsId, tx->msgId, rxTry.len) ;

      memcpy(rx, &rxTry, rxTry.len + 4);
      if (rx->clsId != 0x06) // only cfg has additional ack
        return true ;
    }
    else if ((rxTry.clsId == 0x05) && // ack
        (rxTry.payload[0] == tx->clsId) &&
        (rxTry.payload[1] == tx->msgId))
    {
      ESP_LOGI(TAG, "cmd poll ack: %02x %02x %02x %02x", tx->clsId, tx->msgId, rxTry.clsId, rxTry.msgId) ;
      return found && (rxTry.msgId == 0x01) ; // ack
    }
    else
    {
      ESP_LOGI(TAG, "cmd poll ignore: %02x %02x %02x", rxTry.clsId, rxTry.msgId, rxTry.len) ;
    }
  }
}

bool gps_ubx_cmd_set(gps_ubx_frame_t *tx)
{
  if (!gps_ubx_tx(tx))
    return false ;

  gps_ubx_frame_t rxTry ;
  for (uint8_t try = 0 ; try < 32 ; ++try)
  {
    if (!gps_ubx_rx(&rxTry))
      return false ;

    if ((rxTry.clsId == 0x05) && // ack
        (rxTry.payload[0] == tx->clsId) &&
        (rxTry.payload[1] == tx->msgId))
    {
      ESP_LOGI(TAG, "cmd set ack: %02x %02x %02x %02x", tx->clsId, tx->msgId, rxTry.clsId, rxTry.msgId) ;
      return (rxTry.msgId == 0x01) ; // ack
    }
  }
}

void recv_time(frame_nav_timeutc_t *nav_timeutc)
{
  static bool first = 1 ;

  if (nav_timeutc->valid & 0x04) // valid utc
  {
    struct tm t = {0};

    t.tm_year   = nav_timeutc->year - 1900;
    t.tm_mon    = nav_timeutc->month - 1;
    t.tm_mday   = nav_timeutc->day ;
    t.tm_hour   = nav_timeutc->hour ;
    t.tm_min    = nav_timeutc->min ;
    t.tm_sec    = nav_timeutc->sec ;

    int64_t utc_us = mktime(&t) * 1000000;

    offset_utc_to_sys_tick = utc_us - sys_tick_at_last_pps ;
    gps_time_active = 1 ;

    if (first)
    {
      first = 0 ;
      struct timeval now = { .tv_sec = utc_us / 1000000, .tv_usec = utc_us % 1000000 };
      settimeofday(&now, NULL);
    }
  }
  else
  {
    gps_time_active = 0 ;
  }

  gps_recv_time = 0 ;
}

void recv_pos()
{
  if ((last_nav_status.iTOW != last_nav_posllh.iTOW) ||
      (last_nav_status.iTOW != last_nav_velned.iTOW))
    return ;

  memset((void*)&gps_data, 0, sizeof(gps_data)) ;

  if (last_nav_status.flags & 0x01) // gps fix ok
  {
    gps_data.latitude = last_nav_posllh.lat ;
    gps_data.longitude = last_nav_posllh.lon ;
    gps_data.altitudeWGS = last_nav_posllh.alt ;
    gps_data.altitudeMSL = last_nav_posllh.hMSL ;
    gps_data.speed = last_nav_velned.gSpeed ;
    gps_data.heading = last_nav_velned.heading ;
    gps_pos_active = 1 ;
  }
  else
  {
    gps_pos_active = 0 ;
  }
}

void gps_ubx_read_task(void *pvParameters)
{
  gps_ubx_frame_t rx ;

  while (true)
  {
    if (!gps_ubx_rx(&rx))
      continue ;

    switch (rx.clsId)
    {
    case 0x01: // nav
      {
        switch (rx.msgId)
        {
        case 0x02: // posllh
          {
            ESP_LOGI(TAG, "nav posllh");
            last_nav_posllh = rx.nav_posllh ;
            recv_pos() ;
          }
          break ;
        case 0x03: // status
          {
            ESP_LOGI(TAG, "nav status");
            last_nav_status = rx.nav_status ;
            recv_pos() ;
          }
          break ;
        case 0x12: // velned
          {
            ESP_LOGI(TAG, "nav velned");
            last_nav_velned = rx.nav_velned ;
            recv_pos() ;
          }
          break ;
        case 0x21: // timeutc
          {
            ESP_LOGI(TAG, "nav timeutc");
            gps_recv_time |= GPS_RECV_NAV_TIMEUTC ;
            if (gps_recv_time == GPS_RECV_TIME)
              recv_time(&rx.nav_timeutc) ;
          }
          break ;
        default:
          {
            ESP_LOGI(TAG, "nav ??? %02x", rx.msgId);
          }
          break ;
        }
      }
      break ;
    default:
      {
        ESP_LOGI(TAG, "??? %02x %02x", rx.clsId, rx.msgId);
      }
      break ;
    }

  }
}

bool gps_ubx_baudrate_try(uint32_t baud, gps_ubx_frame_t *rx)
{
  ESP_LOGI(TAG, "try baudrate %d", baud);

  ESP_ERROR_CHECK(uart_wait_tx_done(UART_PORT_NUM, pdMS_TO_TICKS(100)));
  ESP_ERROR_CHECK(uart_set_baudrate(UART_PORT_NUM, baud)) ;
  ESP_ERROR_CHECK(uart_flush(UART_PORT_NUM)) ;

  gps_ubx_frame_t tx = { 6, 0, 0 } ;

  if (!gps_ubx_cmd_poll(&tx, rx))
    return false ;
}

bool gps_ubx_baudrate_try_all(gps_ubx_frame_t *rx)
{
  for (uint8_t try = 0 ; try < 3 ; ++try)
  {
    if (gps_ubx_baudrate_try(  9600, rx)) return true ;
    if (gps_ubx_baudrate_try(115200, rx)) return true ;
    if (gps_ubx_baudrate_try( 19200, rx)) return true ;
    if (gps_ubx_baudrate_try( 38400, rx)) return true ;
    if (gps_ubx_baudrate_try( 57600, rx)) return true ;
  }
  return false ;
}

bool gps_ubx_init()
{
  gps_ubx_frame_t frame ;

  // switch baud rate to 115200

  if (!gps_ubx_baudrate_try_all(&frame))
  {
    ESP_LOGW(TAG, "get baudrate failed") ;
    info_log_data("ubx: not found\n") ;
    return false ;
  }

  ESP_LOGI(TAG, "baudrate found %d", frame.cfg_prt.baudRate) ;

  if (frame.cfg_prt.baudRate != 115200)
  {
    ESP_LOGI(TAG, "set baudrate %d", frame.cfg_prt.baudRate) ;
    frame.cfg_prt.baudRate = 115200 ;
    gps_ubx_cmd_set(&frame) ;
    if (!gps_ubx_baudrate_try(115200, &frame))
    {
      ESP_LOGW(TAG, "set baudrate failed") ;
      gps_ubx_baudrate_try(9600, &frame) ;
      info_log_data("ubx: not found\n") ;
      return false ;
    }
  }

  // switch to ubx protocol

  ESP_LOGI(TAG, "set protocol ubx") ;
  frame.cfg_prt.inProtoMask = 0x01 ;
  frame.cfg_prt.outProtoMask = 0x01 ;
  if (!gps_ubx_cmd_set(&frame))
  {
    ESP_LOGW(TAG, "set protocol failed") ;
    info_log_data("ubx: not found\n") ;
    return false ;
  }

  // request hw info

  ESP_LOGI(TAG, "request info") ;
  frame.clsId = 0x0a ; // mon
  frame.msgId = 0x04 ; // ver
  frame.len = 0 ;

  if (gps_ubx_cmd_poll(&frame, &frame))
  {
    ESP_LOGW(TAG, "sw version: %s", frame.payload +  0) ;
    info_log_data("ubx sw version: %s\n", frame.payload +  0) ;
    ESP_LOGW(TAG, "hw version: %s", frame.payload + 30) ;
    info_log_data("ubx hw version: %s\n", frame.payload +  30) ;
    for (uint16_t offset = 40 ; offset+30 <= frame.len ; offset += 30)
    {
      ESP_LOGW(TAG, "extension: %s", frame.payload + offset) ;
      info_log_data("ubx extension: %s\n", frame.payload +  offset) ;
    }
  }
  else
  {
    ESP_LOGW(TAG, "request info failed") ;
    info_log_data("ubx: no info found\n") ;
  }

  // request ubx messages

  ESP_LOGI(TAG, "request ubx messages") ;
  frame.clsId = 0x06 ; // cfg
  frame.msgId = 0x01 ; // msg
  frame.len = 3 ;
  frame.payload[0] = 0x01 ;
  frame.payload[2] = 0x01 ;

  frame.payload[1] = 0x03 ; // nav status
  if (!gps_ubx_cmd_set(&frame))
    ESP_LOGW(TAG, "request nav status failed") ;

  frame.payload[1] = 0x02 ; // nav posllh
  if (!gps_ubx_cmd_set(&frame))
    ESP_LOGW(TAG, "request nav posllh failed") ;

  frame.payload[1] = 0x12 ; // nav velned
  if (!gps_ubx_cmd_set(&frame))
    ESP_LOGW(TAG, "request nav velned failed") ;

  frame.payload[1] = 0x21 ; // nav timeutc
  if (!gps_ubx_cmd_set(&frame))
    ESP_LOGW(TAG, "request nav timeutc failed") ;

  //frame.payload[1] = 0x07 ; // nav pvt
  //if (!gps_ubx_cmd_set(&frame))
  //  ESP_LOGW(TAG, "request nav pvt failed") ;

  return true ;
}
