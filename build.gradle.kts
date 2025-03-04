// Top-level build file where you can add configuration options common to all sub-projects/modules.
plugins {
    alias(libs.plugins.android.library) apply false
}

tasks.register("copyZygiskFiles") {
    doLast {
        val moduleFolder = project.rootDir.resolve("module")

        val zygiskModule = project.project(":zygisk")
        val zygiskBuildDir = zygiskModule.layout.buildDirectory.get().asFile

        val classesJar = zygiskBuildDir
            .resolve("intermediates/aar_main_jar/release/syncReleaseLibJars/classes.jar")
        classesJar.copyTo(moduleFolder.resolve("classes.jar"), overwrite = true)

        val zygiskSoDir = zygiskBuildDir
            .resolve("intermediates/stripped_native_libs/release/stripReleaseDebugSymbols/out/lib")

        zygiskSoDir.walk()
            .filter { it.isFile && it.name == "libzygisk.so" }
            .forEach { soFile ->
                val abiFolder = soFile.parentFile.name
                val destination = moduleFolder.resolve("zygisk/$abiFolder.so")
                soFile.copyTo(destination, overwrite = true)
            }
    }
}

tasks.register("copyInjectFiles") {
    doLast {
        val moduleFolder = project.rootDir.resolve("module")

        val injectModule = project.project(":inject")
        val injectBuildDir = injectModule.layout.buildDirectory.get().asFile

        val injectSoDir = injectBuildDir
            .resolve("intermediates/stripped_native_libs/release/stripReleaseDebugSymbols/out/lib")

        injectSoDir.walk()
            .filter { it.isFile && it.name == "libinject.so" }
            .forEach { soFile ->
                val abiFolder = soFile.parentFile.name
                val destination = moduleFolder.resolve("inject/$abiFolder.so")
                soFile.copyTo(destination, overwrite = true)
            }
    }
}

tasks.register<Zip>("zip") {
    dependsOn("copyZygiskFiles", "copyInjectFiles")

    archiveFileName.set("PlayIntegrityFix.zip")
    destinationDirectory.set(project.rootDir.resolve("out"))

    from(project.rootDir.resolve("module"))
}
