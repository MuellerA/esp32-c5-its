#include <arpa/inet.h>

#include "log-cvt.h"

PcapWriter::PcapWriter()
{
}
PcapWriter::~PcapWriter()
{
}

bool PcapWriter::open(const std::string &logName)
{
  std::string pcapName = logName + ".pcapng" ;
  _writer = make_unique<fpcap::pcapng::StreamPcapNgWriter>(pcapName, false, 105);
  return true ;
}

bool PcapWriter::close()
{
  _writer.reset() ;
  return true ;
}

bool PcapWriter::writeIts(uint64_t timeUs, const std::vector<uint8_t> &data)
{
  fpcap::Packet packet{};

  packet.data = data.data();
  packet.captureLength = data.size();
  packet.length = data.size();
  
  packet.timestampSeconds = timeUs / 1'000'000 ;
  packet.timestampMicroseconds = timeUs % 1'000'000 ;

  _writer->write(packet);
  return true ;
}

uint32_t crc32(const uint8_t *data, size_t size)
{
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < size; ++i)
  {
    crc ^= data[i];
    for (int j = 0; j < 8; ++j)
    {
      if (crc & 1)
        crc = (crc >> 1) ^ 0xEDB88320;
      else
        crc >>= 1;
    }
  }
  return ~crc;
}

bool PcapWriter::writeGps(uint64_t timeUs, const std::vector<uint8_t> &data)
{
#pragma pack(push, 1)
  struct GpsLogData
  {
    uint8_t quality;
    int32_t latitude;
    int32_t longitude;
    int32_t altitude;
  } ;
#pragma pack(pop)
  
  static uint8_t beacon[] =
  {
    0x88, 0x00, 0x00, 0x00,  0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xAA, 0xAA,  0xAA, 0xAA, 0xAA, 0xAA,
    0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xd0, 0x39,  0x20, 0x00, 0xaa, 0xaa,  0x03, 0x00, 0x00, 0x00,
    0x89, 0x47, 0x11, 0x00,  0x1a, 0x01, 0x00, 0x10,  0x03, 0x00, 0x00, 0x00,  0x01, 0x00, 0x00, 0x00,
    0xAA, 0xAA, 0xAA, 0xAA,  0xAA, 0xAA, 0x11, 0x11,  0x11, 0x11, 0x22, 0x22,  0x22, 0x22, 0x33, 0x33,
    0x33, 0x33, 0x00, 0x00,  0x00, 0x00, 0x44, 0x44,  0x44, 0x44
  } ;

  if (data.size() != sizeof(GpsLogData))
    return false ;
    
  GpsLogData *gpsLogData = (GpsLogData*)data.data() ;

  *(uint32_t*)(beacon + 0x36) = htonl((uint32_t)(timeUs / 1000)) ;
  *(int32_t*)(beacon + 0x3a) = htonl(gpsLogData->latitude) ;
  *(int32_t*)(beacon + 0x3e) = htonl(gpsLogData->longitude) ;

  uint32_t fcs = crc32(beacon, sizeof(beacon)-sizeof(uint32_t));
  *(uint32_t*)(beacon + 0x46) = htonl(fcs) ;

  fpcap::Packet packet{};

  packet.data = beacon;
  packet.captureLength = sizeof(beacon);
  packet.length = sizeof(beacon);
  
  packet.timestampSeconds = timeUs / 1'000'000 ;
  packet.timestampMicroseconds = timeUs % 1'000'000 ;

  _writer->write(packet);
  return true ;  
}