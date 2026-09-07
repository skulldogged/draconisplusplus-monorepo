#include <Drac++/Core/System.hpp>
int main() {
  draconis::utils::cache::CacheManager cache;
  auto memory = draconis::core::system::GetMemInfo(cache);
  return memory && memory->totalBytes > 0 ? 0 : 1;
}
