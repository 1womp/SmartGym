#include "lwip/pbuf.h"
#include "lwip/netif.h"

extern "C" int lwip_hook_ip6_input(struct pbuf* p, struct netif* inp) {
  (void)p;
  (void)inp;
  return 0;
}
