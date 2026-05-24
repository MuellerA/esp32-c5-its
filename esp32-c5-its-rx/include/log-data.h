#pragma once

#include <stdint.h>

#define LOG_DATA_VERSION 1

#define LOG_DATA_TYPE_TIME      0
#define LOG_DATA_TYPE_ITS       1
#define LOG_DATA_TYPE_GPS       2
#define LOG_DATA_TYPE_INFO    254
#define LOG_DATA_TYPE_VERSION 255

#define MAX_PKT_SIZE 2000
#pragma pack(push, 1)

struct Its
{
  uint8_t payload[MAX_PKT_SIZE] ;
} ;

struct Gps
{
  int32_t  latitude;    // °, * 10^7
  int32_t  longitude;   // °, * 10^7
  int32_t  altitudeWGS; // m, * 10^3
  int32_t  altitudeMSL; // m, * 10^3
  int32_t  speed;       // m/s, * 10^2
  int32_t  heading;     // °, * 10^5
} ;

struct Info
{
  char    text[500];
} ;

struct Version
{
  uint8_t logVersion;   // 1
  uint8_t prgVerMaj;
  uint8_t prgVerMin;
  uint8_t prgVerRev;
} ;

struct Header
{
  uint8_t  pkt_type;     // log_data_type_t
  uint8_t  reserved;     // 0
  uint16_t body_size;    // body size  (Time 0, ITS var, GPS 21, Info 6)
  int64_t  timestamp_us; // UTC time in µs after GPS fix, else ESP system time in µs
} ;

union Body
{
  struct Its     its;
  struct Gps     gps;
  struct Info    info;
  struct Version version;
} ;

struct LogData
{
  struct Header header ;
  union Body body ;
} ;

#pragma pack(pop)
