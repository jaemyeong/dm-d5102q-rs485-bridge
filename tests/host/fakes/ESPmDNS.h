#pragma once
#include "Arduino.h"
struct FakeMdns {
  bool beginOk = true, serviceOk = true;
  unsigned begins = 0, ends = 0, services = 0;
  std::string hostname;
  bool begin(const char* name) { ++begins; hostname = name; return beginOk; }
  bool addService(const char* service, const char* proto, uint16_t port) {
    if (strcmp(service, "http") || strcmp(proto, "tcp") || port != 80) __builtin_trap();
    ++services; return serviceOk;
  }
  void end() { ++ends; }
};
extern FakeMdns MDNS;
