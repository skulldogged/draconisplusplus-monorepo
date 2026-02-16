package draconis

data class ResourceUsage(val usedBytes: Long, val totalBytes: Long)
data class CpuCores(val physical: Int, val logical: Int)
data class OsInfo(val name: String, val version: String, val id: String)

data class DiskInfo(
    val name: String,
    val mountPoint: String,
    val filesystem: String,
    val driveType: String,
    val totalBytes: Long,
    val usedBytes: Long,
    val isSystemDrive: Boolean,
)

data class DisplayInfo(
    val id: Long,
    val width: Long,
    val height: Long,
    val refreshRate: Double,
    val isPrimary: Boolean,
)

data class NetworkInterface(
    val name: String,
    val ipv4Address: String?,
    val ipv6Address: String?,
    val macAddress: String?,
    val isUp: Boolean,
    val isLoopback: Boolean,
)

enum class BatteryStatus {
    UNKNOWN,
    CHARGING,
    DISCHARGING,
    FULL,
    NOT_PRESENT;

    companion object {
        fun fromCode(code: Int): BatteryStatus = entries.getOrElse(code) { UNKNOWN }
    }
}

data class Battery(
    val status: BatteryStatus,
    val percentage: Int?,
    val timeRemainingSecs: Long?,
)

