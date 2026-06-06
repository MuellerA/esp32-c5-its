#include "esp32-c5-its-otm.h"

std::atomic<bool> shutdown(false) ;

void signal_handler(int signal)
{
  if (signal == SIGINT)
  {
    std::cout << "\nSIGINT received. Exiting ...\n" ;
    shutdown = true ;
  }
}

#define MQTT_DEFAULT_ID "esp32-c5-its"
#if 0
Options options // test
{
  "/dev/ttyACM0",
  "broker.emqx.io",
  8883,
  MQTT_DEFAULT_ID,
  "testtopic/" MQTT_DEFAULT_ID "/packet",
  "",
  1
} ;
#else
Options options // real
{
  "/dev/ttyACM0",
  "cits1.opentrafficmap.org",
  8883,
  MQTT_DEFAULT_ID,
  "its/" MQTT_DEFAULT_ID "/packet",
  "",
  1
} ;
#endif

std::string machineId()
{
  std::vector<std::string> paths{"/etc/machine-id", "/var/lib/dbus/machine-id"} ;

  std::string mid ;
  for (const auto &path : paths)
  {
    std::ifstream f(path) ;
    if (!f.is_open())
      continue ;

    std::getline(f, mid) ;
    if (mid.size() > 6)
      mid = mid.erase(0, mid.size() - 6) ;
    return mid ;
  }
  return {} ;
}

void usage()
{
  std::cout
    << "usage: esp32-c5-its-otm [option] ...\n"
    << "options\n"
    << "  -t|--tty <device>         tty device (default " << options._ttyName << ")\n"
    << "  -mh|--mqtt-host <host>    mqtt host (default " << options._mqttHost << ")\n"
    << "  -mp|--mqtt-port <port>    mqtt port (default " << options._mqttPort << ")\n"
    << "  -mi|--mqtt-id <id>        mqtt id (default " << options._mqttId << ")\n"
    << "  -mt|--mqtt-topic <topic>  mqtt topic (default " << options._mqttTopicPacket << ")\n"
    << "  --tls                     tls" << ((options._tls == 1) ? " (default)\n" : "\n")
    << "  --tls-unsecure            tls but no verification" << ((options._tls == 2) ? " (default)\n" : "\n")
    << "  --tls-off                 no tls" << ((options._tls == 0) ? " (default)\n" : "\n")
    ;
}

int main(int argc, char *argv[])
{
  std::signal(SIGINT, signal_handler);

  for (int iArg = 1 ; iArg < argc ; ++iArg)
  {
    std::string arg = argv[iArg] ;
    if ((arg == "-t") || (arg == "--tty"))
    {
      if (++iArg == argc)
      {
        usage() ;
        return 1 ;
      }
      options._ttyName = argv[iArg] ;
    }
    else if ((arg == "-mh") || (arg == "--mqtt-host"))
    {
      if (++iArg == argc)
      {
        usage() ;
        return 1 ;
      }
      options._mqttHost = argv[iArg] ;
    }
    else if ((arg == "-mp") || (arg == "--mqtt-port"))
    {
      if (++iArg == argc)
      {
        usage() ;
        return 1 ;
      }
      options._mqttPort = std::stoul(argv[iArg]) ;
    }
    else if ((arg == "-mi") || (arg == "--mqtt-id"))
    {
      if (++iArg == argc)
      {
        usage() ;
        return 1 ;
      }
      options._mqttId = argv[iArg] ;
    }
    else if ((arg == "-mt") || (arg == "--mqtt-topic"))
    {
      if (++iArg == argc)
      {
        usage() ;
        return 1 ;
      }
      options._mqttTopicPacket = argv[iArg] ;
    }
    else if (arg == "--tls")
    {
      options._tls = 1 ;
    }
    else if (arg == "--tls-unsecure")
    {
      options._tls = 2 ;
    }
    else if (arg == "--tls-off")
    {
      options._tls = 0 ;
    }
    else
    {
      usage() ;
      return 1 ;
    }
  }

  std::string mid = machineId() ;
  if (mid.size())
    options._mqttId += "-" + mid ;
  std::cout << "mid " << mid << " " << options._mqttId << "\n" ;

  size_t pos = options._mqttTopicPacket.find(MQTT_DEFAULT_ID) ;
  if (pos != std::string::npos)
    options._mqttTopicPacket.replace(pos, strlen(MQTT_DEFAULT_ID), options._mqttId) ;

  pos = options._mqttTopicPacket.find("/packet") ;
  if (pos != std::string::npos)
  {
    options._mqttTopicStatus = options._mqttTopicPacket ;
    options._mqttTopicStatus.replace(pos, strlen("/packet"), "/status") ;
  }

  QueueTty queueTty ;
  QueueIts queueIts ;

  Reader reader(queueTty) ;
  Parser parser(queueTty, queueIts) ;
  Writer writer(queueIts) ;

  if (!reader.start() ||
      !parser.start() ||
      !writer.start())
    shutdown = true ;

  reader.stop() ;
  parser.stop() ;
  writer.stop() ;

  return 0 ;
}