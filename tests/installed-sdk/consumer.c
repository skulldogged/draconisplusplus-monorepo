#include <draconis_c.h>
int main(void) {
  DracCacheManager* cache = DracCreateCacheManager();
  DracResourceUsage memory = {0};
  int result = DracGetMemInfo(cache, &memory);
  DracDestroyCacheManager(cache);
  return result == DRAC_SUCCESS && memory.totalBytes > 0 ? 0 : 1;
}
