#define PAGE_SIZE 4096

namespace NLCBSMM {

  uint32_t     CLONE_STACK_SZ   = 4096 * 32;
  uint32_t     PAGE_TABLE_SZ    = 4096 * 16;

  uint16_t     UNICAST_PORT     = 60000;
  uint16_t     MULTICAST_PORT   = 60001;

  const char*  MULTICAST_GRP    = "225.0.0.6";

}
