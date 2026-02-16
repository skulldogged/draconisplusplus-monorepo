import java.util.Properties

plugins {
    kotlin("jvm") version "1.9.25"
}

group = "dev.draconis"
version = "0.1.0"

repositories {
    mavenCentral()
}

java {
    toolchain {
        languageVersion.set(JavaLanguageVersion.of(21))
    }
}

tasks.withType<JavaCompile>().configureEach {
    sourceCompatibility = "21"
    targetCompatibility = "21"
}

tasks.withType<org.jetbrains.kotlin.gradle.tasks.KotlinCompile>().configureEach {
    kotlinOptions {
        jvmTarget = "21"
    }
}

dependencies {
    testImplementation(kotlin("test"))
}

val dracConfigPath = System.getenv("DRAC_CONFIG_PATH")
    ?: providers.environmentVariable("DRAC_CONFIG_PATH").orNull

fun loadDracConfig(): Properties {
    val path = dracConfigPath ?: error("DRAC_CONFIG_PATH not set. Run via Meson or set it to drac_kotlin_paths.ini.")
    val props = Properties()
    // java.util.Properties treats backslashes as escapes, which breaks Windows paths.
    file(path).readLines().forEach { line ->
        val trimmed = line.trim()
        if (trimmed.isEmpty() || trimmed.startsWith("#")) return@forEach
        val idx = trimmed.indexOf('=')
        if (idx <= 0) return@forEach
        val key = trimmed.substring(0, idx).trim()
        val value = trimmed.substring(idx + 1).trim()
        props.setProperty(key, value)
    }
    return props
}

// CMake tasks removed - JNI library is built by Meson


val runExample by tasks.registering(JavaExec::class) {
    group = "application"
    description = "Run Kotlin example (like other bindings)"
    dependsOn("classes")

    mainClass.set("draconis.examples.MainKt")
    classpath = sourceSets["main"].runtimeClasspath

    doFirst {
         val cfg = loadDracConfig()
         val jniLibDir = cfg.getProperty("JNI_LIB_DIR") ?: error("JNI_LIB_DIR not found in config")
         jvmArgs("-Djava.library.path=$jniLibDir")
    }
}
