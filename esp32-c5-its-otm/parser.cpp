#include "esp32-c5-its-otm.h"

template <class T>
bool read(QueueTty &queue, T &v)
{
  uint8_t *data = (uint8_t*)&v ;
  for (size_t i = 0 ; i < sizeof(v) ; ++i)
  {
    uint8_t b ;
    if (!queue.pop(b))
      return false ;
    data[i] = b ;
  }
  return true ;
}

bool read(QueueTty &queue, size_t size, std::vector<uint8_t> &v)
{
  v.resize(size) ;
  uint8_t *data = v.data() ;
  for (size_t i = 0 ; i < size ; ++i)
  {
    uint8_t b ;
    if (!queue.pop(b))
      return false ;
    data[i] = b ;
  }
  return true ;
}

bool skip_magic(QueueTty &queue)
{
  uint8_t c ;
  uint32_t magic{0} ;

  while (read(queue, c))
  {
    magic <<= 8 ;
    magic |= c ;
    if (magic == 0xaa5555aa)
      return true ;
  }
  return false ;
}


bool Parser::start(QueueTty &queueTty, QueueIts &queueIts)
{
  _thread = std::thread([this, &queueTty, &queueIts]()
  {
    Header header ;
    std::vector<uint8_t> body ;

    while (!shutdown)
    {
      if (!skip_magic(queueTty))
        return ;

      if (!read(queueTty, header))
        return ;

      if (header.body_size > 2000)
        continue ;

      if (!read(queueTty, header.body_size, body))
        return ;

      std::cout << "\r\033[2K" << (uint32_t)header.pkt_type << " " << header.timestamp_us << " " << header.body_size ;
      std::cout.flush() ;

      switch (header.pkt_type)
      {
      case LOG_DATA_TYPE_TIME:
        break ;
      case LOG_DATA_TYPE_ITS:
        queueIts.push(std::move(body)) ;
        break ;
      case LOG_DATA_TYPE_GPS:
        break ;
      case LOG_DATA_TYPE_INFO:
      std::cout
        << "\nInfo:\n"
        << std::string((char*)body.data(), body.size()) ;
        break ;
      case LOG_DATA_TYPE_VERSION:
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
      }
    }
   }) ;

  return true ;
}

void Parser::stop()
{
  if (_thread.joinable())
    _thread.join() ;
}
