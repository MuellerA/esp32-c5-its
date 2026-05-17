#include "esp32-c5-its-otm.h"



bool Reader::initPort()
{
  _port = open(options._ttyName.c_str(), O_RDWR | O_NOCTTY);

  if (_port < 0) {
      std::cerr << "open failed " << options._ttyName << ": " << strerror(errno) << std::endl;
      return false;
  }

  struct termios tty;
  if (tcgetattr(_port, &tty) != 0) {
      std::cerr << "tcgetattr failed: " << strerror(errno) << std::endl;
      close(_port);
      return false;
  }

  cfmakeraw(&tty);
  tty.c_cflag |= HUPCL ;
  tty.c_cflag &= ~CRTSCTS;   
  tty.c_cflag |= CREAD | CLOCAL;
  tty.c_cc[VMIN] = 0;
  tty.c_cc[VTIME] = 10;

  cfsetispeed(&tty, B115200);
  cfsetospeed(&tty, B115200);

  if (tcsetattr(_port, TCSANOW, &tty) != 0)
  {
    std::cerr << "tcsetattr failed: " << strerror(errno) << std::endl;
    close(_port);
    return false;
  }

  int flags;
  if (ioctl(_port, TIOCMGET, &flags) >= 0)
  {
    flags &= ~TIOCM_DTR;
    flags |= TIOCM_RTS;
    ioctl(_port, TIOCMSET, &flags);
 
    std::this_thread::sleep_for(std::chrono::milliseconds(50));  

    flags |= TIOCM_DTR;
    flags |= TIOCM_RTS;
    ioctl(_port, TIOCMSET, &flags);
 
    std::this_thread::sleep_for(std::chrono::milliseconds(50));      
  }
  else
  {
    std::cout << "ioctl failed: " << strerror(errno) << std::endl ;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  if (tcflush(_port, TCIFLUSH) != 0)
  {
    std::cerr << "tcflush failed: " << strerror(errno) << std::endl;
  }

  return true ;
}

bool Reader::start(QueueTty &queue)
{
  if (!initPort())
    return false ;

  _thread = std::thread([this, &queue]()
  {
    uint8_t buff[256];
    while (!shutdown)
    {
      ssize_t nBuff = read(_port, &buff, sizeof(buff));

      if (nBuff < 0) {
        std::cerr << "read failed: " << strerror(errno) << std::endl;
        break;
      }

      for (ssize_t iBuff = 0; iBuff < nBuff; ++iBuff) {
        queue.push(buff[iBuff]) ;
      }
    }
  }) ;

  return true ;
}

void Reader::stop()
{
  if (_thread.joinable())
    _thread.join() ;

  close(_port);
}