#include "esp32-c5-its-otm.h"

 Writer::Writer() : _mqtt(options._mqttId.c_str())
 {
 }

bool Writer::start(QueueIts &queueIts)
{
  mosqpp::lib_init() ;

  int rc = _mqtt.loop_start();
  if (rc != MOSQ_ERR_SUCCESS) {
    std::cerr << "mqtt loop start failed: " << mosqpp::strerror(rc) << std::endl;
    return false ;
  }

  switch (options._tls)
  {
    default:
      break ;
    case 1:
      _mqtt.tls_set("/etc/ssl/certs/ca-certificates.crt", nullptr, nullptr, nullptr, nullptr);
      break ;
    case 2:
      _mqtt.tls_set("/etc/ssl/certs/ca-certificates.crt", nullptr, nullptr, nullptr, nullptr);
      _mqtt.tls_opts_set(SSL_VERIFY_NONE, nullptr, nullptr);
      _mqtt.tls_insecure_set(true);
      break ;
  }
  rc = _mqtt.connect(options._mqttHost.c_str(), options._mqttPort, 60);
  if (rc != MOSQ_ERR_SUCCESS)
  {
    std::cerr << "mqtt connect failed: " << mosqpp::strerror(rc) << std::endl;
    return false ;
  }

  _thread = std::thread([this, &queueIts]()
  {
    std::vector<uint8_t> data ;

    while (!shutdown)
    {
      if (!queueIts.pop(data))
        continue ;

      std::cout << data.size() << "\n" ;

      int mid ;
      int rc = _mqtt.publish(
        &mid,
        options._mqttTopic.c_str(),
        static_cast<int>(data.size()),
        data.data(),
        1,
        false
    );
    }
  }) ;

  return true ;
}

void Writer::stop()
{
  if (_thread.joinable())
    _thread.join() ;

  _mqtt.disconnect() ;
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  _mqtt.loop_stop(true) ;
  mosqpp::lib_cleanup() ;
}