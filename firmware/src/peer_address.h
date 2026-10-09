#pragma once
#include <stdint.h>
#include <string.h>
#ifdef ARDUINO
#include <lwip/sockets.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

// ESP-IDF's dual-stack HTTP listener reports IPv4 clients as ::ffff:a.b.c.d.
// Keep the whole sockaddr; truncating it to sockaddr_in reads the flow label
// as the IPv4 destination. The audio receiver currently supports IPv4 only.
inline bool peerIPv4(const sockaddr_storage& storage, socklen_t length, uint32_t& address) {
  if(storage.ss_family==AF_INET&&length>=sizeof(sockaddr_in)) {
    address=reinterpret_cast<const sockaddr_in*>(&storage)->sin_addr.s_addr;
    return true;
  }
  if(storage.ss_family==AF_INET6&&length>=sizeof(sockaddr_in6)) {
    const auto& a=reinterpret_cast<const sockaddr_in6*>(&storage)->sin6_addr;
    const auto* bytes=reinterpret_cast<const uint8_t*>(&a);
    const uint8_t mapped[12]={0,0,0,0,0,0,0,0,0,0,0xff,0xff};
    if(memcmp(bytes,mapped,sizeof(mapped))!=0)return false;
    memcpy(&address,bytes+12,4);
    return true;
  }
  return false;
}
