#include <cstdio>
#include <cstdint>

#include <iostream>
#include <vector>

#include "log-cvt.h"


Writer::Writer() {}
Writer::~Writer() {}

bool Writer::open(const std::string &) { return true ; }
bool Writer::close() { return true ; }
bool Writer::writeIts(uint64_t, const std::vector<uint8_t>&) { return true ; }
bool Writer::writeGps(uint64_t, const std::vector<uint8_t>&) { return true ; }

template <class t>
bool read(FILE *log, t &v)
{
  if (fread(&v, sizeof(t), 1, log) != 1)
  {
    return false ;
  }
  return true ;
}

bool read(FILE *log, size_t size, std::vector<uint8_t> &v)
{
  v.resize(size) ;
  if (fread(v.data(), sizeof(uint8_t), v.size(), log) != v.size())
  {
    return false ;
  }
  return true ;
}

bool skip_magic(FILE *log)
{
  uint8_t c ;
  uint32_t magic{0} ;

  while (read(log, c))
  {
    magic <<= 8 ;
    magic |= c ;
    if (magic == 0xaa5555aa)
      return true ;
  }
  return false ;
}


bool log_cvt(FILE *log, std::unique_ptr<Writer> &writer, ContentCount &count)
{
  Header header ;
  std::vector<uint8_t> body ;

  if (!read(log, header))
    return false ;

  if (!read(log, header.body_size, body))
    return false ;

  switch (header.pkt_type)
  {
  case LOG_DATA_TYPE_TIME: // ignore
    count.time += 1 ;
    break ;
  case LOG_DATA_TYPE_ITS:
    if (writer->writeIts(header.timestamp_us, body))
      count.its += 1 ;
    break ;
  case LOG_DATA_TYPE_GPS:
    if (writer->writeGps(header.timestamp_us, body))
      count.gps += 1 ;
    break ;
  case LOG_DATA_TYPE_INFO:
    count.info += 1 ;
    std::cout
      << "\nInfo:\n"
      << std::string((char*)body.data(), body.size()) ;
    break ;
  case LOG_DATA_TYPE_VERSION:
    count.version += 1 ;
    {
      if (body.size() < sizeof(Version))
        break ;
      const Version &version = *(Version*)body.data() ;
      std::cout
        << "\nVersion:\n"
        << "Log: " << (int) version.logVersion << "\n"
        << "Prg: " << (int) version.prgVerMaj << "." << (int) version.prgVerMin << "." << (int) version.prgVerRev << "\n" ;
    }
    break ;
  default:
    count.unknown += 1 ;
    std::cerr << "unknown type: " << (int) header.pkt_type << "\n" ;
    return false ;
  }

  return true ;
}

void usage()
{
  std::cout
    << "usage: log-cvt [-p|-b] [-u] <log-file> ...\n"
    << "options:\n"
    << "  -p    pcapng file (default)\n"
    << "  -b    vector blf file\n"
    << "  -u    source is usb log with magic bytes (default sd log w/o magic bytes)\n" ;
}

int main(int argc, char *argv[])
{
  int argi ;
  enum class Type { none, pcapng, blf } ;
  Type type{Type::none} ;
  int isUsbLog{0} ;

  for (argi = 1 ; argi < argc && argv[argi][0] == '-' ; ++argi)
  {
    std::string arg = argv[argi] ;
    if (arg == "-b")
      type = Type::blf ;
    else if (arg == "-p")
      type = Type::pcapng ;
    else if (arg == "-u")
      isUsbLog = 1 ;
    else
    {
      usage() ;
      return 1 ;
    }
  }

  if (argi == argc)
  {
    usage() ;
    return 1 ;
  }

  for ( ; argi < argc ; ++argi)
  {
    std::string logName = argv[argi] ;

    FILE *log = fopen(logName.c_str(), "rb") ;
    if (!log)
    {
      std::cerr << "open log " << logName << " failed\n" ;
      continue ;
    }

    std::unique_ptr<Writer> writer ;
    switch (type)
    {
    case Type::pcapng:
      writer = std::make_unique<PcapWriter>() ;
      break ;
    case Type::blf:
      writer = std::make_unique<BlfWriter>() ;
      break ;
    default:
      writer = std::make_unique<Writer>() ;
    }

    if (!writer->open(logName))
    {
      fclose(log) ;
      continue ;
    }

    std::cout
      << "\nFile: " << logName << "\n" ;

    ContentCount count ;
    while (!feof(log))
    {
      if ((isUsbLog && !skip_magic(log)) ||
          !log_cvt(log, writer, count))
        break ;
    }

    std::cout
      << "\nCount:\n"
      // << "time: " << count.time << "\n"
      << "its: " << count.its << "\n"
      << "gps: " << count.gps << "\n"
      << "info: " << count.info << "\n"
      << "version: " << count.version << "\n" ;

    writer->close() ;
    fclose(log) ;
  }
  return 0 ;
}