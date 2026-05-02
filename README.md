# ESP32-C5 ITS Logger

Logger for Intelligent Transort Systems (ITS) G5 / IEEE 802.11p / V2X / Car2X messages at 5.9GHz.

Inspired by the [OpenTrafficMap](https://opentrafficmap.org/) project to use an ESP32-C5 outside its specs to log ITS messages. While OpenTrafficMap focuses on stationary measurement points to record traffic on a map, this project's focus is recording while moving.

A mobile phone can be used for power supply and to record the data via USB. Logged are the raw messages, decoding has to be done with other tools e.g. Wireshark.

Transmitting at 5.9GHz is illigal in most countries. As ITS messages are for Safety and everybody should be safe, receiving and logging might be ok. Check yourself for your country and use it on your own risk.

## ESP32-C5-ITS-RX

ESP32C5 firmware. Captures ITS messages and logs them to the Serial/JTAG USB (CDC/ACM).

It's an PlatformIO project, compile and upload with PlatformIO. Unofficial functions are used to enable 802.11p communication, they might be removed from the espidf in later versions. 

Optionally a GPS receiver can be connected for time and position.

| ESP32C5 | GPS |
|---------|-----|
| GPIO 3  | PPS |
| GPIO 4  | RX  |
| GPIO 5  | TX  |

## ESP32-C5-ITS-Log

Converter for the recorded data to 
- PCAPNG: Generated file name is source name + ".pcapng" extension. GPS data is added as a Beacon message with source address { 0xaa 0xaa 0xaa 0xaa 0xaa 0xaa }. Uses the [fpcap](https://github.com/fpcap/fpcap) library.
- BLF: Generated file name is source name + ".blf" extension. GPS data is added as GNSS system variable. Uses the [Technica-Engineering vector_blf](https://github.com/Technica-Engineering/vector_blf) library.

The source is tested on Linux. Download and compile fpcap and vector_blf libraries. Update the library paths in the Makefile and run make.

```txt
usage: log-cvt [-p|-b] <log-file> ...
options:
  -p    pcapng file (default)
  -b    vector blf file
```

## Log File Format

Format is still in development, compatibility may break at any time.

```c
struct
{
  uint32_t magic;        // 0xAA5555AA
  uint8_t  pkt_type;     // 1 ITS / 2 GPS
  uint64_t timestamp_us; // UTC time in µs
  uint16_t length;       // length of follwing data (varibiable for ITS, 13 for GPS)

  union
  {
    struct
    {
      uint8_t payload[length]; // its message
    } its;
    struct
    {
      uint8_t quality;
      int32_t latitude;
      int32_t longitude;
      int32_t altitude;
    } gps;
  };
} 
```