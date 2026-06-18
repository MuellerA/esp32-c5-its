#pragma once

#if defined(GPS)

extern volatile uint8_t gps_recv_time ;

extern volatile struct Gps gps_data ;

extern volatile int64_t sys_tick_at_last_pps ;

extern volatile int64_t offset_utc_to_sys_tick ;

extern void gps_nmea_read_task(void *pvParameters) ;

extern bool gps_ubx_init() ;
extern void gps_ubx_read_task(void *pvParameters) ;

#endif