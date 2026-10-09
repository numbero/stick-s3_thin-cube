#include "../src/peer_address.h"
#include <assert.h>
#include <stdio.h>

int main() {
  const uint8_t ip[4]={192,168,0,101};
  uint32_t expected=0,address=0;memcpy(&expected,ip,4);
  sockaddr_storage storage{};
  auto* v4=reinterpret_cast<sockaddr_in*>(&storage);
  v4->sin_family=AF_INET;v4->sin_addr.s_addr=expected;
  assert(peerIPv4(storage,sizeof(*v4),address)&&address==expected);
  assert(!peerIPv4(storage,sizeof(*v4)-1,address));
  storage={};auto* v6=reinterpret_cast<sockaddr_in6*>(&storage);
  v6->sin6_family=AF_INET6;
  auto* bytes=reinterpret_cast<uint8_t*>(&v6->sin6_addr);
  bytes[10]=bytes[11]=0xff;memcpy(bytes+12,ip,4);
  assert(peerIPv4(storage,sizeof(*v6),address)&&address==expected);
  assert(!peerIPv4(storage,sizeof(sockaddr_in),address));
  bytes[10]=bytes[11]=0;bytes[0]=0x20;bytes[1]=0x01;
  assert(!peerIPv4(storage,sizeof(*v6),address));
  puts("5 peer address scenarios passed");
}
