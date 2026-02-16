package draconis.examples

import draconis.CacheManager

fun main() {
    CacheManager().use { mgr ->
        println("Uptime (s): ${CacheManager.uptimeSeconds()}")
        println("Host: ${mgr.host()}")
        println("OS: ${mgr.operatingSystem()}")
        println("Kernel: ${mgr.kernelVersion()}")
        println("Desktop Env: ${mgr.desktopEnvironment()}")
        println("Window Manager: ${mgr.windowManager()}")
        println("Shell: ${mgr.shell()}")
        println("CPU Model: ${mgr.cpuModel()}")
        println("GPU Model: ${mgr.gpuModel()}")
        println("CPU Cores: ${mgr.cpuCores()}")
        println("Memory: ${mgr.memInfo()}")
        println("Disk Usage: ${mgr.diskUsage()}")
        println("System Disk: ${mgr.systemDisk()}")
        println("Disks: ${mgr.disks()}")
        println("Primary Output: ${mgr.primaryOutput()}")
        println("Outputs: ${mgr.outputs()}")
        println("Primary Network Interface: ${mgr.primaryNetworkInterface()}")
        println("Network Interfaces: ${mgr.networkInterfaces()}")
        println("Battery: ${mgr.batteryInfo()}")
    }
}

