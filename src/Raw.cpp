#include "Raw.h"
#include <AsyncTCP.h>
#include <vector>

namespace {
  AsyncServer rawSrv(82);
  std::vector<AsyncClient*> clients;

  void addClient(AsyncClient* c) {
    clients.push_back(c);
    c->onDisconnect([](void* arg, AsyncClient* c2){ 
      clients.erase(std::remove(clients.begin(), clients.end(), c2), clients.end());
    }, nullptr);
  }
}

namespace Raw {
void begin() {
  rawSrv.onClient([](void*, AsyncClient* c){ addClient(c); }, nullptr);
  rawSrv.begin();
}

void broadcastLine(const String& s) {
  for (auto* c: clients) if (c && c->connected()) c->write((const char*)s.c_str(), s.length());
}
}
