package draconis

class CacheManager : AutoCloseable {
    private var handle: Long = nativeCreateManager().also {
        check(it != 0L) { "Failed to create native CacheManager" }
    }

    private fun requireHandle(): Long {
        check(handle != 0L) { "CacheManager is closed" }
        return handle
    }

    @Synchronized
    fun memInfo(): ResourceUsage {
        val arr = nativeGetMemInfo(requireHandle())
        return ResourceUsage(arr[0], arr[1])
    }

    @Synchronized
    fun cpuCores(): CpuCores {
        val arr = nativeGetCpuCores(requireHandle())
        return CpuCores(arr[0], arr[1])
    }

    @Synchronized
    fun operatingSystem(): OsInfo {
        val arr = nativeGetOperatingSystem(requireHandle())
        return OsInfo(
            name = arr[0] ?: "",
            version = arr[1] ?: "",
            id = arr[2] ?: "",
        )
    }

    @Synchronized
    fun desktopEnvironment(): String? = nativeGetDesktopEnvironment(requireHandle())

    @Synchronized
    fun windowManager(): String? = nativeGetWindowManager(requireHandle())

    @Synchronized
    fun shell(): String? = nativeGetShell(requireHandle())

    @Synchronized
    fun host(): String? = nativeGetHost(requireHandle())

    @Synchronized
    fun cpuModel(): String? = nativeGetCPUModel(requireHandle())

    @Synchronized
    fun gpuModel(): String? = nativeGetGPUModel(requireHandle())

    @Synchronized
    fun kernelVersion(): String? = nativeGetKernelVersion(requireHandle())

    @Synchronized
    fun diskUsage(): ResourceUsage {
        val arr = nativeGetDiskUsage(requireHandle())
        return ResourceUsage(arr[0], arr[1])
    }

    @Synchronized
    fun disks(): List<DiskInfo> = nativeGetDisks(requireHandle()).toList()

    @Synchronized
    fun systemDisk(): DiskInfo = nativeGetSystemDisk(requireHandle())

    @Synchronized
    fun outputs(): List<DisplayInfo> = nativeGetOutputs(requireHandle()).toList()

    @Synchronized
    fun primaryOutput(): DisplayInfo = nativeGetPrimaryOutput(requireHandle())

    @Synchronized
    fun networkInterfaces(): List<NetworkInterface> = nativeGetNetworkInterfaces(requireHandle()).toList()

    @Synchronized
    fun primaryNetworkInterface(): NetworkInterface = nativeGetPrimaryNetworkInterface(requireHandle())

    @Synchronized
    fun batteryInfo(): Battery {
        val arr = nativeGetBatteryInfo(requireHandle())
        val status = BatteryStatus.fromCode(arr[0].toInt())
        val pct = arr[1].toInt().takeIf { it != 255 }
        val time = arr[2].takeIf { it != -1L }
        return Battery(status, pct, time)
    }

    @Synchronized
    override fun close() {
        val previous = handle
        handle = 0L
        if (previous != 0L) nativeDestroyManager(previous)
    }

    private external fun nativeCreateManager(): Long
    private external fun nativeDestroyManager(handle: Long)
    private external fun nativeGetMemInfo(handle: Long): LongArray
    private external fun nativeGetCpuCores(handle: Long): IntArray
    private external fun nativeGetOperatingSystem(handle: Long): Array<String?>
    private external fun nativeGetDesktopEnvironment(handle: Long): String?
    private external fun nativeGetWindowManager(handle: Long): String?
    private external fun nativeGetShell(handle: Long): String?
    private external fun nativeGetHost(handle: Long): String?
    private external fun nativeGetCPUModel(handle: Long): String?
    private external fun nativeGetGPUModel(handle: Long): String?
    private external fun nativeGetKernelVersion(handle: Long): String?
    private external fun nativeGetDiskUsage(handle: Long): LongArray
    private external fun nativeGetDisks(handle: Long): Array<DiskInfo>
    private external fun nativeGetSystemDisk(handle: Long): DiskInfo
    private external fun nativeGetOutputs(handle: Long): Array<DisplayInfo>
    private external fun nativeGetPrimaryOutput(handle: Long): DisplayInfo
    private external fun nativeGetNetworkInterfaces(handle: Long): Array<NetworkInterface>
    private external fun nativeGetPrimaryNetworkInterface(handle: Long): NetworkInterface
    private external fun nativeGetBatteryInfo(handle: Long): LongArray

    companion object {
        init {
            System.loadLibrary("draconis_jni")
        }

        fun uptimeSeconds(): Long = nativeGetUptime()

        @JvmStatic private external fun nativeGetUptime(): Long
    }
}
