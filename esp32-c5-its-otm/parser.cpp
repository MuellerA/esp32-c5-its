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
    uint8_t type ;
    uint64_t timeUs ;
    uint16_t size ;
    std::vector<uint8_t> data ;
  
    while (!shutdown)
    {
      if (!skip_magic(queueTty))
        return ;

      if (!read(queueTty, type))
        return ;
      if (!read(queueTty, timeUs))
        return ;
      if (!read(queueTty, size))
        return ;

      if (size > 2000)
        continue ;
       
      if (!read(queueTty, size, data))
        return ;

      std::cout << (uint32_t)type << " " << timeUs << " " << size << " " << data.size() << "\n" ;

      if (type == 1)
        queueIts.push(std::move(data)) ;
    }
  }) ;

  return true ;
}

void Parser::stop()
{
  if (_thread.joinable())
    _thread.join() ;
}
