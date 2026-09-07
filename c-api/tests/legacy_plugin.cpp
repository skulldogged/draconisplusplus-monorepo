#ifdef _WIN32
  #define EXPORT __declspec(dllexport)
#else
  #define EXPORT __attribute__((visibility("default")))
#endif
// Loading must reject this old plugin before invoking any factory.
extern "C" EXPORT void* CreatePlugin() {
  return nullptr;
}
extern "C" EXPORT void DestroyPlugin(void*) {}
