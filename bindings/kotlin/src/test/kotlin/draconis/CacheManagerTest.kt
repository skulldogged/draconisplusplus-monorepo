package draconis

import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors
import kotlin.test.Test
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

class CacheManagerTest {
    @Test fun closeIsIdempotentAndEveryOperationRejectsClosedHandles() {
        CacheManager().use { manager ->
            assertTrue(manager.memInfo().totalBytes > 0)
            manager.close()
            manager.close()
            val operations: List<() -> Any?> = listOf(
                { manager.memInfo() }, { manager.cpuCores() }, { manager.operatingSystem() },
                { manager.desktopEnvironment() }, { manager.windowManager() }, { manager.shell() },
                { manager.host() }, { manager.cpuModel() }, { manager.gpuModel() },
                { manager.kernelVersion() }, { manager.diskUsage() }, { manager.disks() },
                { manager.systemDisk() }, { manager.outputs() }, { manager.primaryOutput() },
                { manager.networkInterfaces() }, { manager.primaryNetworkInterface() }, { manager.batteryInfo() },
            )
            operations.forEach { operation -> assertFailsWith<IllegalStateException> { operation() } }
            assertTrue(CacheManager.uptimeSeconds() >= 0)
        }
    }

    @Test fun closeIsSerializedWithNativeCalls() {
        val executor = Executors.newFixedThreadPool(2)
        try {
            repeat(25) {
                val manager = CacheManager()
                val start = CountDownLatch(1)
                val reader = executor.submit {
                    start.await()
                    try { manager.memInfo() } catch (_: IllegalStateException) { }
                }
                val closer = executor.submit { start.await(); manager.close() }
                start.countDown()
                reader.get(); closer.get(); manager.close()
            }
        } finally { executor.shutdown() }
    }
}
