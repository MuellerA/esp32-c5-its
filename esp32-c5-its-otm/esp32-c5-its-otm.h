#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>
#include <cstdint>
#include <csignal>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <mosquittopp.h>
#include <openssl/ssl.h> 

extern std::atomic<bool> shutdown ;

struct Options
{
  std::string _ttyName ;
  std::string _mqttHost ;
  uint32_t    _mqttPort ;
  std::string _mqttId ;
  std::string _mqttTopic ;
  uint32_t    _tls ; // 0 no tls, 1 tls, 2 tls w/o vfy
} ;
extern Options options ;

template<class T, size_t Size>
class Queue
{
public:
  bool push(T value) ;
  bool pop(T &value) ;
  bool empty() ;

private:
  std::queue<T> _queue;
  std::mutex _mutex;
  std::condition_variable _cv;
} ;

template<class T, size_t Size>
bool Queue<T,Size>::push(T value) {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_queue.size() > Size)
    return false;
  _queue.push(std::move(value));
  _cv.notify_one();
  return true ;
}

template<class T, size_t Size>
bool Queue<T,Size>::pop(T &value) {
  std::unique_lock<std::mutex> lock(_mutex);
  while (true)
  {
    bool success = _cv.wait_for(lock, std::chrono::seconds(1), [this] { return !_queue.empty(); });
    if (!success)
    {
      if (shutdown)
        return false ;
      continue ;
    }
    value = std::move(_queue.front());
    _queue.pop();
    return true;
  }
}
template<class T, size_t Size>
bool Queue<T,Size>::empty() {
    std::lock_guard<std::mutex> lock(_mutex);
    return _queue.empty();
}

using QueueTty = Queue<uint8_t, 2000> ;
using QueueIts = Queue<std::vector<uint8_t>, 64> ;

class Reader
{
public:
  bool start(QueueTty &queue) ;
  void stop() ;

private:
  bool initPort() ;
  std::thread _thread ;
  int _port ;
} ;

class Parser
{
public:
  bool start(QueueTty &queueTty, QueueIts &queueIts) ;
  void stop() ;

private:
  std::thread _thread ;
} ;

class Writer
{
public:
  Writer() ;
  bool start(QueueIts &queueIts) ;
  void stop() ;

private:
  std::thread _thread ;
  mosqpp::mosquittopp _mqtt ;
} ;
