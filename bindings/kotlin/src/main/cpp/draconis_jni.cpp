#include <jni.h>
#include <span>
#include <string>

#include "draconis_c.h"

namespace {
  auto throwOnError(JNIEnv* env, DracErrorCode code, const char* context) -> void {
    if (code == DRAC_SUCCESS)
      return;
    jclass exClass = env->FindClass("java/lang/RuntimeException");
    if (!exClass)
      return;
    std::string msg = std::string(context) + " failed with code " + std::to_string((int)code);
    env->ThrowNew(exClass, msg.c_str());
  }

  auto toJString(JNIEnv* env, const char* cstr) -> jstring {
    if (!cstr)
      return nullptr;
    return env->NewStringUTF(cstr);
  }

  template <typename Fn>
  auto getStringResult(JNIEnv* env, DracCacheManager* mgr, Fn func, const char* context) -> jstring {
    char* out  = nullptr;
    auto  code = func(mgr, &out);
    throwOnError(env, code, context);
    if (env->ExceptionCheck())
      return nullptr;
    jstring jstr = toJString(env, out);
    if (out)
      DracFreeString(out);
    return jstr;
  }

  template <typename T>
  auto fromHandle(jlong handle) -> T* {
    // NOLINTNEXTLINE(performance-no-int-to-ptr, cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<T*>(handle);
  }

  auto toHandle(void* ptr) -> jlong {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<jlong>(ptr);
  }
} // namespace

// NOLINTBEGIN(readability-identifier-naming)
extern "C" {
  JNIEXPORT auto JNICALL Java_draconis_CacheManager_nativeCreateManager(JNIEnv* /*env*/, jobject /*obj*/) -> jlong {
    DracCacheManager* mgr = DracCreateCacheManager();
    return toHandle(mgr);
  }

  JNIEXPORT auto JNICALL Java_draconis_CacheManager_nativeDestroyManager(JNIEnv* /*env*/, jobject /*obj*/, jlong handle) -> void {
    auto* mgr = fromHandle<DracCacheManager>(handle);

    if (mgr)
      DracDestroyCacheManager(mgr);
  }

  JNIEXPORT auto JNICALL Java_draconis_CacheManager_nativeGetUptime(JNIEnv* /*env*/, jclass /*cls*/) -> jlong {
    return static_cast<jlong>(DracGetUptime());
  }

  JNIEXPORT auto JNICALL Java_draconis_CacheManager_nativeGetMemInfo(JNIEnv* env, jobject /*obj*/, jlong handle) -> jlongArray {
    auto*             mgr = fromHandle<DracCacheManager>(handle);
    DracResourceUsage usage {};
    auto              code = DracGetMemInfo(mgr, &usage);
    throwOnError(env, code, "DracGetMemInfo");
    if (env->ExceptionCheck())
      return nullptr;

    jlongArray arr     = env->NewLongArray(2);
    jlong      vals[2] = {
      static_cast<jlong>(usage.usedBytes),
      static_cast<jlong>(usage.totalBytes),
    };
    env->SetLongArrayRegion(arr, 0, 2, vals);
    return arr;
  }

  JNIEXPORT auto JNICALL Java_draconis_CacheManager_nativeGetCpuCores(JNIEnv* env, jobject /*obj*/, jlong handle) -> jintArray {
    auto*        mgr = fromHandle<DracCacheManager>(handle);
    DracCPUCores cores {};
    auto         code = DracGetCpuCores(mgr, &cores);
    throwOnError(env, code, "DracGetCpuCores");
    if (env->ExceptionCheck())
      return nullptr;

    jintArray arr     = env->NewIntArray(2);
    jint      vals[2] = {
      static_cast<jint>(cores.physical),
      static_cast<jint>(cores.logical),
    };
    env->SetIntArrayRegion(arr, 0, 2, vals);
    return arr;
  }

  JNIEXPORT auto JNICALL Java_draconis_CacheManager_nativeGetOperatingSystem(JNIEnv* env, jobject /*obj*/, jlong handle) -> jobjectArray {
    auto*      mgr = fromHandle<DracCacheManager>(handle);
    DracOSInfo info {};
    auto       code = DracGetOperatingSystem(mgr, &info);
    throwOnError(env, code, "DracGetOperatingSystem");
    if (env->ExceptionCheck())
      return nullptr;

    jclass       stringClass = env->FindClass("java/lang/String");
    jobjectArray arr         = env->NewObjectArray(3, stringClass, nullptr);

    if (info.name)
      env->SetObjectArrayElement(arr, 0, env->NewStringUTF(info.name));

    if (info.version)
      env->SetObjectArrayElement(arr, 1, env->NewStringUTF(info.version));

    if (info.id)
      env->SetObjectArrayElement(arr, 2, env->NewStringUTF(info.id));

    DracFreeOSInfo(&info);
    return arr;
  }

  JNIEXPORT auto JNICALL Java_draconis_CacheManager_nativeGetDesktopEnvironment(JNIEnv* env, jobject /*obj*/, jlong handle) -> jstring {
    auto* mgr = fromHandle<DracCacheManager>(handle);
    return getStringResult(env, mgr, DracGetDesktopEnvironment, "DracGetDesktopEnvironment");
  }

  JNIEXPORT auto JNICALL Java_draconis_CacheManager_nativeGetWindowManager(JNIEnv* env, jobject /*obj*/, jlong handle) -> jstring {
    auto* mgr = fromHandle<DracCacheManager>(handle);
    return getStringResult(env, mgr, DracGetWindowManager, "DracGetWindowManager");
  }

  JNIEXPORT auto JNICALL Java_draconis_CacheManager_nativeGetShell(JNIEnv* env, jobject /*obj*/, jlong handle) -> jstring {
    auto* mgr = fromHandle<DracCacheManager>(handle);
    return getStringResult(env, mgr, DracGetShell, "DracGetShell");
  }

  JNIEXPORT auto JNICALL Java_draconis_CacheManager_nativeGetHost(JNIEnv* env, jobject /*obj*/, jlong handle) -> jstring {
    auto* mgr = fromHandle<DracCacheManager>(handle);
    return getStringResult(env, mgr, DracGetHost, "DracGetHost");
  }

  JNIEXPORT auto JNICALL Java_draconis_CacheManager_nativeGetCPUModel(JNIEnv* env, jobject /*obj*/, jlong handle) -> jstring {
    auto* mgr = fromHandle<DracCacheManager>(handle);
    return getStringResult(env, mgr, DracGetCPUModel, "DracGetCPUModel");
  }

  JNIEXPORT auto JNICALL Java_draconis_CacheManager_nativeGetGPUModel(JNIEnv* env, jobject /*obj*/, jlong handle) -> jstring {
    auto* mgr = fromHandle<DracCacheManager>(handle);
    return getStringResult(env, mgr, DracGetGPUModel, "DracGetGPUModel");
  }

  JNIEXPORT auto JNICALL Java_draconis_CacheManager_nativeGetKernelVersion(JNIEnv* env, jobject /*obj*/, jlong handle) -> jstring {
    auto* mgr = fromHandle<DracCacheManager>(handle);
    return getStringResult(env, mgr, DracGetKernelVersion, "DracGetKernelVersion");
  }

  JNIEXPORT auto JNICALL Java_draconis_CacheManager_nativeGetDiskUsage(JNIEnv* env, jobject /*obj*/, jlong handle) -> jlongArray {
    auto*             mgr = fromHandle<DracCacheManager>(handle);
    DracResourceUsage usage {};
    auto              code = DracGetDiskUsage(mgr, &usage);
    throwOnError(env, code, "DracGetDiskUsage");
    if (env->ExceptionCheck())
      return nullptr;

    jlongArray arr     = env->NewLongArray(2);
    jlong      vals[2] = { static_cast<jlong>(usage.usedBytes), static_cast<jlong>(usage.totalBytes) };
    env->SetLongArrayRegion(arr, 0, 2, vals);
    return arr;
  }

  JNIEXPORT auto JNICALL Java_draconis_CacheManager_nativeGetDisks(JNIEnv* env, jobject /*obj*/, jlong handle) -> jobjectArray {
    auto*            mgr = fromHandle<DracCacheManager>(handle);
    DracDiskInfoList list {};
    auto             code = DracGetDisks(mgr, &list);
    throwOnError(env, code, "DracGetDisks");
    if (env->ExceptionCheck())
      return nullptr;

    jclass    diskClass = env->FindClass("draconis/DiskInfo");
    jmethodID ctor      = env->GetMethodID(
      diskClass, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;JJZ)V"
    );
    jobjectArray arr = env->NewObjectArray((jsize)list.count, diskClass, nullptr);

    auto items = std::span(list.items, list.count);
    for (size_t i = 0; i < items.size(); ++i) {
      const auto& disk = items[i];
      jobject     obj  = env->NewObject(
        diskClass, ctor, toJString(env, disk.name), toJString(env, disk.mountPoint), toJString(env, disk.filesystem), toJString(env, disk.driveType), (jlong)disk.totalBytes, (jlong)disk.usedBytes, (jboolean)disk.isSystemDrive
      );
      env->SetObjectArrayElement(arr, (jsize)i, obj);
    }

    DracFreeDiskInfoList(&list);
    return arr;
  }

  JNIEXPORT auto JNICALL Java_draconis_CacheManager_nativeGetSystemDisk(JNIEnv* env, jobject /*obj*/, jlong handle) -> jobject {
    auto*        mgr = fromHandle<DracCacheManager>(handle);
    DracDiskInfo disk {};
    auto         code = DracGetSystemDisk(mgr, &disk);
    throwOnError(env, code, "DracGetSystemDisk");
    if (env->ExceptionCheck())
      return nullptr;

    jclass    diskClass = env->FindClass("draconis/DiskInfo");
    jmethodID ctor      = env->GetMethodID(
      diskClass, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;JJZ)V"
    );
    jobject obj = env->NewObject(
      diskClass, ctor, toJString(env, disk.name), toJString(env, disk.mountPoint), toJString(env, disk.filesystem), toJString(env, disk.driveType), (jlong)disk.totalBytes, (jlong)disk.usedBytes, (jboolean)disk.isSystemDrive
    );
    DracFreeDiskInfo(&disk);
    return obj;
  }

  JNIEXPORT auto JNICALL Java_draconis_CacheManager_nativeGetOutputs(JNIEnv* env, jobject /*obj*/, jlong handle) -> jobjectArray {
    auto*               mgr = fromHandle<DracCacheManager>(handle);
    DracDisplayInfoList list {};
    auto                code = DracGetOutputs(mgr, &list);
    throwOnError(env, code, "DracGetOutputs");
    if (env->ExceptionCheck())
      return nullptr;

    jclass       displayClass = env->FindClass("draconis/DisplayInfo");
    jmethodID    ctor         = env->GetMethodID(displayClass, "<init>", "(JJJDZ)V");
    jobjectArray arr          = env->NewObjectArray((jsize)list.count, displayClass, nullptr);

    auto items = std::span(list.items, list.count);
    for (size_t i = 0; i < items.size(); ++i) {
      const auto& display = items[i];
      jobject     obj     = env->NewObject(
        displayClass, ctor, (jlong)display.id, (jlong)display.width, (jlong)display.height, (jdouble)display.refreshRate, (jboolean)display.isPrimary
      );
      env->SetObjectArrayElement(arr, (jsize)i, obj);
    }

    DracFreeDisplayInfoList(&list);
    return arr;
  }

  JNIEXPORT auto JNICALL Java_draconis_CacheManager_nativeGetPrimaryOutput(JNIEnv* env, jobject /*obj*/, jlong handle) -> jobject {
    auto*           mgr = fromHandle<DracCacheManager>(handle);
    DracDisplayInfo display {};
    auto            code = DracGetPrimaryOutput(mgr, &display);
    throwOnError(env, code, "DracGetPrimaryOutput");
    if (env->ExceptionCheck())
      return nullptr;

    jclass    displayClass = env->FindClass("draconis/DisplayInfo");
    jmethodID ctor         = env->GetMethodID(displayClass, "<init>", "(JJJDZ)V");
    return env->NewObject(
      displayClass, ctor, (jlong)display.id, (jlong)display.width, (jlong)display.height, (jdouble)display.refreshRate, (jboolean)display.isPrimary
    );
  }

  JNIEXPORT auto JNICALL Java_draconis_CacheManager_nativeGetNetworkInterfaces(JNIEnv* env, jobject /*obj*/, jlong handle) -> jobjectArray {
    auto*                    mgr = fromHandle<DracCacheManager>(handle);
    DracNetworkInterfaceList list {};
    auto                     code = DracGetNetworkInterfaces(mgr, &list);
    throwOnError(env, code, "DracGetNetworkInterfaces");
    if (env->ExceptionCheck())
      return nullptr;

    jclass    ifaceClass = env->FindClass("draconis/NetworkInterface");
    jmethodID ctor       = env->GetMethodID(
      ifaceClass, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;ZZ)V"
    );
    jobjectArray arr = env->NewObjectArray((jsize)list.count, ifaceClass, nullptr);

    auto items = std::span(list.items, list.count);
    for (size_t i = 0; i < items.size(); ++i) {
      const auto& iface = items[i];
      jobject     obj   = env->NewObject(
        ifaceClass, ctor, toJString(env, iface.name), toJString(env, iface.ipv4Address), toJString(env, iface.ipv6Address), toJString(env, iface.macAddress), (jboolean)iface.isUp, (jboolean)iface.isLoopback
      );
      env->SetObjectArrayElement(arr, (jsize)i, obj);
    }

    DracFreeNetworkInterfaceList(&list);
    return arr;
  }

  JNIEXPORT auto JNICALL Java_draconis_CacheManager_nativeGetPrimaryNetworkInterface(JNIEnv* env, jobject /*obj*/, jlong handle) -> jobject {
    auto*                mgr = fromHandle<DracCacheManager>(handle);
    DracNetworkInterface iface {};
    auto                 code = DracGetPrimaryNetworkInterface(mgr, &iface);
    throwOnError(env, code, "DracGetPrimaryNetworkInterface");
    if (env->ExceptionCheck())
      return nullptr;

    jclass    ifaceClass = env->FindClass("draconis/NetworkInterface");
    jmethodID ctor       = env->GetMethodID(
      ifaceClass, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;ZZ)V"
    );
    jobject obj = env->NewObject(
      ifaceClass, ctor, toJString(env, iface.name), toJString(env, iface.ipv4Address), toJString(env, iface.ipv6Address), toJString(env, iface.macAddress), (jboolean)iface.isUp, (jboolean)iface.isLoopback
    );
    DracFreeNetworkInterface(&iface);
    return obj;
  }

  JNIEXPORT auto JNICALL Java_draconis_CacheManager_nativeGetBatteryInfo(JNIEnv* env, jobject /*obj*/, jlong handle) -> jlongArray {
    auto*       mgr = fromHandle<DracCacheManager>(handle);
    DracBattery bat {};
    auto        code = DracGetBatteryInfo(mgr, &bat);
    throwOnError(env, code, "DracGetBatteryInfo");
    if (env->ExceptionCheck())
      return nullptr;

    jlongArray arr     = env->NewLongArray(3);
    jlong      vals[3] = {
      (jlong)bat.status,
      (jlong)bat.percentage,
      (jlong)bat.timeRemainingSecs,
    };
    env->SetLongArrayRegion(arr, 0, 3, vals);
    return arr;
  }

  // Plugin system
  JNIEXPORT auto JNICALL Java_draconis_PluginSystem_nativeInitStaticPlugins(JNIEnv* /*env*/, jclass /*cls*/) -> jlong {
    return static_cast<jlong>(DracInitStaticPlugins());
  }

  JNIEXPORT void JNICALL Java_draconis_PluginSystem_nativeInitPluginManager(JNIEnv* /*env*/, jclass /*cls*/) {
    DracInitPluginManager();
  }

  JNIEXPORT void JNICALL Java_draconis_PluginSystem_nativeShutdownPluginManager(JNIEnv* /*env*/, jclass /*cls*/) {
    DracShutdownPluginManager();
  }

  JNIEXPORT auto JNICALL Java_draconis_Plugin_nativeLoad(JNIEnv* env, jclass /*cls*/, jstring name) -> jlong {
    if (!name) return 0;
    const char* cname = env->GetStringUTFChars(name, nullptr);
    if (!cname) return 0;
    DracPlugin* plugin = DracLoadPlugin(cname);
    env->ReleaseStringUTFChars(name, cname);
    return toHandle(plugin);
  }

  JNIEXPORT void JNICALL Java_draconis_Plugin_nativeUnload(JNIEnv* /*env*/, jobject /*obj*/, jlong handle) {
    auto* plugin = fromHandle<DracPlugin>(handle);
    if (plugin)
      DracUnloadPlugin(plugin);
  }

  JNIEXPORT auto JNICALL Java_draconis_Plugin_nativeInitialize(JNIEnv* env, jobject /*obj*/, jlong pluginHandle, jlong cacheHandle) -> jint {
    auto* plugin = fromHandle<DracPlugin>(pluginHandle);
    auto* cache  = fromHandle<DracCacheManager>(cacheHandle);
    if (!plugin || !cache)
      return DRAC_ERROR_INVALID_ARGUMENT;
    return static_cast<jint>(DracPluginInitialize(plugin, cache));
  }

  JNIEXPORT auto JNICALL Java_draconis_Plugin_nativeIsEnabled(JNIEnv* /*env*/, jobject /*obj*/, jlong handle) -> jboolean {
    auto* plugin = fromHandle<DracPlugin>(handle);
    if (!plugin) return JNI_FALSE;
    return DracPluginIsEnabled(plugin) ? JNI_TRUE : JNI_FALSE;
  }

  JNIEXPORT auto JNICALL Java_draconis_Plugin_nativeIsReady(JNIEnv* /*env*/, jobject /*obj*/, jlong handle) -> jboolean {
    auto* plugin = fromHandle<DracPlugin>(handle);
    if (!plugin) return JNI_FALSE;
    return DracPluginIsReady(plugin) ? JNI_TRUE : JNI_FALSE;
  }

  JNIEXPORT auto JNICALL Java_draconis_Plugin_nativeCollectData(JNIEnv* env, jobject /*obj*/, jlong pluginHandle, jlong cacheHandle) -> jint {
    auto* plugin = fromHandle<DracPlugin>(pluginHandle);
    auto* cache  = fromHandle<DracCacheManager>(cacheHandle);
    if (!plugin || !cache)
      return DRAC_ERROR_INVALID_ARGUMENT;
    return static_cast<jint>(DracPluginCollectData(plugin, cache));
  }

  JNIEXPORT auto JNICALL Java_draconis_Plugin_nativeGetJson(JNIEnv* env, jobject /*obj*/, jlong handle) -> jstring {
    auto* plugin = fromHandle<DracPlugin>(handle);
    if (!plugin) return nullptr;
    char* json = DracPluginGetJson(plugin);
    if (!json) return nullptr;
    jstring result = toJString(env, json);
    DracFreeString(json);
    return result;
  }

  JNIEXPORT auto JNICALL Java_draconis_Plugin_nativeGetFields(JNIEnv* env, jobject /*obj*/, jlong handle) -> jobjectArray {
    auto* plugin = fromHandle<DracPlugin>(handle);
    if (!plugin) return nullptr;
    DracPluginFieldList fields = DracPluginGetFields(plugin);
    if (!fields.items || fields.count == 0) {
      jclass mapClass = env->FindClass("java/util/HashMap");
      jmethodID ctor  = env->GetMethodID(mapClass, "<init>", "()V");
      return env->NewObject(mapClass, ctor);
    }

    jclass    mapClass = env->FindClass("java/util/HashMap");
    jmethodID mapCtor  = env->GetMethodID(mapClass, "<init>", "()V");
    jmethodID put      = env->GetMethodID(mapClass, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    jobject   map      = env->NewObject(mapClass, mapCtor);

    for (size_t i = 0; i < fields.count; ++i) {
      const auto& field = fields.items[i];
      jstring key       = field.key ? toJString(env, field.key) : nullptr;
      jstring value     = field.value ? toJString(env, field.value) : nullptr;
      if (key && value)
        env->CallObjectMethod(map, put, key, value);
    }

    DracFreePluginFieldList(&fields);
    return map;
  }

  JNIEXPORT auto JNICALL Java_draconis_Plugin_nativeGetLastError(JNIEnv* env, jobject /*obj*/, jlong handle) -> jstring {
    auto* plugin = fromHandle<DracPlugin>(handle);
    if (!plugin) return nullptr;
    char* err = DracPluginGetLastError(plugin);
    if (!err) return nullptr;
    jstring result = toJString(env, err);
    DracFreeString(err);
    return result;
  }

  JNIEXPORT void JNICALL Java_draconis_PluginSystem_nativeAddPluginSearchPath(JNIEnv* env, jclass /*cls*/, jstring path) {
    if (!path) return;
    const char* cpath = env->GetStringUTFChars(path, nullptr);
    if (!cpath) return;
    DracAddPluginSearchPath(cpath);
    env->ReleaseStringUTFChars(path, cpath);
  }

  JNIEXPORT auto JNICALL Java_draconis_Plugin_nativeLoadFromPath(JNIEnv* env, jclass /*cls*/, jstring path) -> jlong {
    if (!path) return 0;
    const char* cpath = env->GetStringUTFChars(path, nullptr);
    if (!cpath) return 0;
    DracPlugin* plugin = DracLoadPluginFromPath(cpath);
    env->ReleaseStringUTFChars(path, cpath);
    return toHandle(plugin);
  }
} // extern "C"
// NOLINTEND(readability-identifier-naming)
