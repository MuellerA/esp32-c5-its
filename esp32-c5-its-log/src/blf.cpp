#include <iostream>
#include <cstring>

#include "log-cvt.h"


void BlfWriter::timeReset()
{
  _timeFirst = true ;
  _timeMeasurementStartUs = 0 ;
}

void BlfWriter::utcTimeToBlfTime(uint64_t utcTimeS, Vector::BLF::SYSTEMTIME &blfTime)
{
  struct tm *t = gmtime((time_t*)&utcTimeS) ;

  blfTime.year = t->tm_year + 1900 ;
  blfTime.month = t->tm_mon + 1 ;
  blfTime.day = t->tm_mday ;
  blfTime.dayOfWeek = t->tm_wday ;
  blfTime.hour = t->tm_hour;
  blfTime.minute = t->tm_min;
  blfTime.second = t->tm_sec;
  blfTime.milliseconds = 0;  
}

uint64_t BlfWriter::timeNs(uint64_t timeUs)
{
  uint64_t timeS = timeUs / 1000000 ;

  if (_timeFirst)
  {
    _timeMeasurementStartUs = timeS * 1000000 ;

    utcTimeToBlfTime(timeS, _blf.fileStatistics.measurementStartTime) ;
    
    _timeFirst = false ;
  }

  utcTimeToBlfTime(timeS, _blf.fileStatistics.lastObjectTime) ;

  return (timeUs - _timeMeasurementStartUs) * 1000 ;
}

BlfWriter::BlfWriter()
{
}
BlfWriter::~BlfWriter()
{
}

bool BlfWriter::open(const std::string &logName)
{
  timeReset() ;
  std::string blfName = logName + ".blf" ;
  _blf.open(blfName.c_str(), std::ios_base::out) ;
  if (!_blf.is_open())
  {
    std::cerr << "open blf " << blfName << " failed\n" ;
    return false ;
  }
  return true ;
}

bool BlfWriter::close()
{
  _blf.close() ;
  return true ;
}

bool BlfWriter::writeIts(uint64_t timeUs, const std::vector<uint8_t> &data0)
{
  std::vector<uint8_t> data = data0 ;

  if (data.size() > 32) // add pseudo eth layer
    data.insert(data.begin() + 32, data0.begin() + 4, data0.begin() + 4 + 12) ;

  auto *wlanFrame = new Vector::BLF::WlanFrame ;
  wlanFrame->objectTimeStamp = timeNs(timeUs);

  wlanFrame->channel = 1 ;
  wlanFrame->flags = 7 ;
  wlanFrame->dir = Vector::BLF::WlanFrame::Dir::Rx ;
  wlanFrame->radioChannel = 180 ;
  wlanFrame->frameLength = data.size() ;
  wlanFrame->frameData = data ;

  _blf.write(wlanFrame) ;
  return true ;
}

bool BlfWriter::writeGps(uint64_t timeUs, const std::vector<uint8_t> &data)
{
  const static std::string gnssSysVarName = "::GNSS::Ath1" ;
#pragma pack(push, 1)
  struct GpsLogData
  {
    uint8_t quality;
    int32_t latitude;
    int32_t longitude;
    int32_t altitude;
  } ;
  struct GnssSysVarData
  {
    double latitude ;
    double longitude ;
    double altitude ;
    double speed ;
    double direction ;
    uint32_t mode ;
    uint32_t status ;
    uint32_t satInView ;
    uint32_t satInUse ;
    double gnssTime ;
    double hdop ;
    double pdop ;
    double vdop ;
    double magVar ;
    double geoAlt ;
  } ;
  GnssSysVarData gnssSysVarData ;
#pragma pack(pop)

  if (data.size() != sizeof(GpsLogData))
    return false ;
  
  memset(&gnssSysVarData, 0, sizeof(gnssSysVarData)) ;
  GpsLogData *gpsLogData = (GpsLogData*)data.data() ;

  gnssSysVarData.latitude = (double)gpsLogData->latitude / 1e7 ;
  gnssSysVarData.longitude = (double)gpsLogData->longitude / 1e7;
  gnssSysVarData.altitude = (double)gpsLogData->altitude / 10;
  gnssSysVarData.mode = gpsLogData->quality ? 3 : 0 ;
  gnssSysVarData.status = 0 ;
  gnssSysVarData.satInView = 10 ;
  gnssSysVarData.satInUse = 10 ;
  gnssSysVarData.gnssTime = (double)timeUs / 1e6 ;

  auto gnssSysVar = new Vector::BLF::SystemVariable ;
  gnssSysVar->objectTimeStamp = timeNs(timeUs) ;

  gnssSysVar->type = Vector::BLF::SystemVariable::ByteArray ;
  gnssSysVar->nameLength = gnssSysVarName.size() ;
  gnssSysVar->dataLength = sizeof(GnssSysVarData) ;
  gnssSysVar->name = gnssSysVarName ;
  gnssSysVar->data.assign((uint8_t*)&gnssSysVarData, (uint8_t*)&gnssSysVarData + sizeof(GnssSysVarData)) ;

  _blf.write(gnssSysVar) ;
  return true ;
}
