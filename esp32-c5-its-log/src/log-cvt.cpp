#include <cstdio>
#include <cstdint>

#include <iostream>
#include <vector>

#include "log-cvt.h"


Writer::Writer() {}
Writer::~Writer() {}
bool Writer::writeGps(uint64_t , const std::vector<uint8_t> &) { return true ; }


const uint8_t TypeIts = 1 ;
const uint8_t TypeGps = 2 ;

template <class t>
bool read(FILE *log, t &v)
{
  if (fread(&v, sizeof(t), 1, log) != 1)
  {
    std::cerr << "read failed\n" ;
    return false ;
  }
  return true ;
}

bool read(FILE *log, size_t size, std::vector<uint8_t> &v)
{
  v.resize(size) ;
  if (fread(v.data(), sizeof(uint8_t), v.size(), log) != v.size())
  {
    std::cerr << "read failed\n" ;
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
  std::cerr << "magic failed\n" ;
  return false ;
}


bool log_cvt(FILE *log, std::unique_ptr<Writer> &writer)
{
  uint8_t type ;
  uint64_t timeUs ;
  uint16_t size ;
  std::vector<uint8_t> data ;

  if (!read(log, type))
    return false ;
  if (!read(log, timeUs))
    return false ;
  if (!read(log, size))
    return false ;

  if (!read(log, size, data))
    return false ;
  
  switch (type)
  {
  case TypeIts:
    writer->writeIts(timeUs, data) ;
    break ;
  case TypeGps:
    writer->writeGps(timeUs, data) ;
    break ;
  default:
    std::cerr << "unknown type\n" ;
    return false ;
  }

  return true ;
}

void usage()
{
  std::cout
    << "usage: log-cvt [-p|-b] <log-file> ...\n"
    << "options:\n"
    << "  -p    pcapng file (default)\n"
    << "  -b    vector blf file\n" ;
}

int main(int argc, char *argv[])
{
  int argi ;
  enum class Type { pcapng, blf } ;
  Type type{Type::pcapng} ;

  for (argi = 1 ; argi < argc && argv[argi][0] == '-' ; ++argi)
  {
    std::string arg = argv[argi] ;
    if (arg == "-b")
      type = Type::blf ;
    else if (arg == "-p")
      type = Type::pcapng ;
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
    }

    if (!writer->open(logName))
    {
      fclose(log) ;
      continue ;
    }

    while (!feof(log))
    {
      if (!skip_magic(log) ||
          !log_cvt(log, writer))
        break ;
    }

    writer->close() ;
    fclose(log) ;
  }
  return 0 ;
}