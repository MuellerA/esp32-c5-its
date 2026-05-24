#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

#include <Vector/BLF.h>
//#include <fpcap/fpcap.hpp>
//#include <fpcap/filesystem/Writer.hpp>
#include <fpcap/pcapng/PcapNgWriter.hpp>
#include <fpcap/filesystem/FileWriter.hpp>

#include "log-data.h"

struct ContentCount
{
  uint32_t time{0} ;
  uint32_t its{0} ;
  uint32_t gps{0} ;
  uint32_t info{0} ;
  uint32_t version{0} ;
  uint32_t unknown{0} ;
} ;

class Writer
{
public:
  Writer() ;
  virtual ~Writer() ;

  virtual bool open(const std::string &logName) ;
  virtual bool close() ;

  virtual bool writeIts(uint64_t timeUs, const std::vector<uint8_t> &data) ;
  virtual bool writeGps(uint64_t timeUs, const std::vector<uint8_t> &data) ;
} ;

class BlfWriter : public Writer
{

public:
  BlfWriter() ;
  virtual ~BlfWriter() ;

  virtual bool open(const std::string &logName) override ;
  virtual bool close() override ;

  virtual bool writeIts(uint64_t timeUs, const std::vector<uint8_t> &data) override ;
  virtual bool writeGps(uint64_t timeUs, const std::vector<uint8_t> &data) override ;

private:

  void timeReset() ;
  void utcTimeToBlfTime(uint64_t utcTimeS, Vector::BLF::SYSTEMTIME &blfTime) ;
  uint64_t timeNs(uint64_t timeUs) ;

  Vector::BLF::File _blf ;
  bool     _timeFirst ;
  uint64_t _timeMeasurementStartUs ;
} ;

class PcapWriter : public Writer
{
public:

  PcapWriter() ;
  virtual ~PcapWriter() ;

  virtual bool open(const std::string &logName) override ;
  virtual bool close() override ;

  virtual bool writeIts(uint64_t timeUs, const std::vector<uint8_t> &data) override ;
  virtual bool writeGps(uint64_t timeUs, const std::vector<uint8_t> &data) override ;

private:

  std::unique_ptr<fpcap::pcapng::StreamPcapNgWriter> _writer ;
} ;
