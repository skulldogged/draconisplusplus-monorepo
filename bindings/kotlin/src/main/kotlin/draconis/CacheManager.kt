package draconis

class CacheManager : AutoCloseable {
    private val handle: Long = nativeCreateManager()

    fun memInfo(): ResourceUsage {
        val arr = nativeGetMemInfo(handle)
        return ResourceUsage(arr[0], arr[1])
    }

    fun cpuCores(): CpuCores {
        val arr = nativeGetCpuCores(handle)
        return CpuCores(arr[0], arr[1])
    }

    fun operatingSystem(): OsInfo {
        val arr = nativeGetOperatingSystem(handle)
        return OsInfo(
            name = arr[0] ?: "",
            version = arr[1] ?: "",
            id = arr[2] ?: "",
        )
    }

    fun desktopEnvironment(): String? = nativeGetDesktopEnvironment(handle)

    fun windowManager(): String? = nativeGetWindowManager(handle)

    fun shell(): String? = nativeGetShell(handle)

    fun host(): String? = nativeGetHost(handle)

    fun cpuModel(): String? = nativeGetCPUModel(handle)

    fun gpuModel(): String? = nativeGetGPUModel(handle)

    fun kernelVersion(): String? = nativeGetKernelVersion(handle)

    fun diskUsage(): ResourceUsage {
        val arr = nativeGetDiskUsage(handle)
        return ResourceUsage(arr[0], arr[1])
    }

    fun disks(): List<DiskInfo> = nativeGetDisks(handle).toList()

    fun systemDisk(): DiskInfo = nativeGetSystemDisk(handle)

    fun outputs(): List<DisplayInfo> = nativeGetOutputs(handle).toList()

    fun primaryOutput(): DisplayInfo = nativeGetPrimaryOutput(handle)

    fun networkInterfaces(): List<NetworkInterface> = nativeGetNetworkInterfaces(handle).toList()

    fun primaryNetworkInterface(): NetworkInterface = nativeGetPrimaryNetworkInterface(handle)

    fun batteryInfo(): Battery {
        val arr = nativeGetBatteryInfo(handle)
        val status = BatteryStatus.fromCode(arr[0].toInt())
        val pct = arr[1].toInt().takeIf { it != 255 }
        val time = arr[2].takeIf { it != -1L }
        return Battery(status, pct, time)
    }

    override fun close() {
        nativeDestroyManager(handle)
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
