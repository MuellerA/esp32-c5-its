#include "esp32-c5-its-otm.h"

const std::string statusOnline("online") ;
const std::string statusOffline("offline") ;

Writer::Writer(QueueIts &queueIts) : mosqpp::mosquittopp(options._mqttId.c_str()), _queueIts(queueIts)
{
}

bool Writer::start()
{
  mosqpp::lib_init() ;

  int rc = loop_start();
  if (rc != MOSQ_ERR_SUCCESS) {
    std::cerr << "mqtt loop start failed: " << mosqpp::strerror(rc) << std::endl;
    return false ;
  }

  if (options._mqttTopicStatus.size())
  {
    will_set(options._mqttTopicStatus.c_str(), static_cast<int>(statusOffline.size()), statusOffline.c_str(), 1, true) ;
  }

  switch (options._tls)
  {
    default:
      break ;
    case 1:
      tls_set("/etc/ssl/certs/ca-certificates.crt", nullptr, nullptr, nullptr, nullptr);
      break ;
    case 2:
      tls_set("/etc/ssl/certs/ca-certificates.crt", nullptr, nullptr, nullptr, nullptr);
      tls_opts_set(SSL_VERIFY_NONE, nullptr, nullptr);
      tls_insecure_set(true);
      break ;
  }
  rc = connect(options._mqttHost.c_str(), options._mqttPort, 60);
  if (rc != MOSQ_ERR_SUCCESS)
  {
    std::cerr << "mqtt connect failed: " << mosqpp::strerror(rc) << std::endl;
    return false ;
  }

  return true ;
}

std::atomic<bool> shutdownWriter ;

void Writer::on_connect(int rc)
{
  if (options._mqttTopicStatus.size())
    publish(nullptr, options._mqttTopicStatus.c_str(), static_cast<int>(statusOnline.size()), statusOnline.c_str(), 1, true) ;

  if (_thread.joinable())
  {
    _thread.detach() ;
  }
  shutdownWriter = false;

  _thread = std::thread([this]()
  {
    std::vector<uint8_t> data ;

    _queueIts.clear() ;
    while (!shutdown)
    {
      if (!_queueIts.pop(data))
        continue ;

      if (shutdownWriter)
        return ;

      publish(nullptr, options._mqttTopicPacket.c_str(), static_cast<int>(data.size()), data.data(), 0, false);
    }
  }) ;
}

void Writer::on_disconnect(int rc)
{
  shutdownWriter = true ;
  _queueIts.push({}) ;
}

void Writer::stop()
{
  if (_thread.joinable())
    _thread.join() ;

  if (options._mqttTopicStatus.size())
  {
    publish(nullptr, options._mqttTopicStatus.c_str(), static_cast<int>(statusOffline.size()), statusOffline.c_str(), 1, true) ;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
  }
  disconnect() ;
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  loop_stop(true) ;
  mosqpp::lib_cleanup() ;
}