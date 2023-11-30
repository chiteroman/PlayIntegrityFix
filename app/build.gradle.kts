plugins {
    id("com.android.application")
}

android {
    namespace = "es.chiteroman.playintegrityfix"
    compileSdk = 34
    ndkVersion = "26.1.10909125"
    buildToolsVersion = "34.0.0"

    buildFeatures {
        prefab = true
    }

    packaging {
        jniLibs {
            excludes += "**/libdobby.so"
        }
    }

    defaultConfig {
        applicationId = "es.chiteroman.playintegrityfix"
        minSdk = 26
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"

        externalNativeBuild {
            cmake {
                arguments += "-DANDROID_STL=none"
                arguments += "-DCMAKE_BUILD_TYPE=MinSizeRel"

                cFlags += "-fvisibility=hidden"
                cFlags += "-fvisibility-inlines-hidden"
                cFlags += "-flto"

                cppFlags += "-std=c++20"
                cppFlags += "-fno-exceptions"
                cppFlags += "-fno-rtti"
                cppFlags += "-fvisibility=hidden"
                cppFlags += "-fvisibility-inlines-hidden"
                cppFlags += "-flto"
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}

dependencies {
    implementation("dev.rikka.ndk.thirdparty:cxx:1.2.0")
}

tasks.register("copyFiles") {
    doLast {
        val moduleFolder = project.rootDir.resolve("module")
        val dexFile = project.buildDir.resolve("intermediates/dex/release/minifyReleaseWithR8/classes.dex")
        val soDir = project.buildDir.resolve("intermediates/stripped_native_libs/release/out/lib")

        dexFile.copyTo(moduleFolder.resolve("classes.dex"), overwrite = true)

        soDir.walk().filter { it.isFile && it.extension == "so" }.forEach { soFile ->
            val abiFolder = soFile.parentFile.name
            val destination = moduleFolder.resolve("zygisk/$abiFolder.so")
            soFile.copyTo(destination, overwrite = true)
        }
    }
}

tasks.register("copyFiles-resetprop") {
    doLast {
        val moduleFolder = project.rootDir.resolve("module_resetprop")
        val dexFile = project.buildDir.resolve("intermediates/dex/release/minifyReleaseWithR8/classes.dex")
        val soDir = project.buildDir.resolve("intermediates/stripped_native_libs/release/out/lib")

        dexFile.copyTo(moduleFolder.resolve("classes.dex"), overwrite = true)

        soDir.walk().filter { it.isFile && it.extension == "so" }.forEach { soFile ->
            val abiFolder = soFile.parentFile.name
            val destination = moduleFolder.resolve("zygisk/$abiFolder.so")
            soFile.copyTo(destination, overwrite = true)
        }
    }
}

tasks.register<Zip>("zip") {
    dependsOn("copyFiles")

    archiveFileName.set("PlayIntegrityFix.zip")
    destinationDirectory.set(project.rootDir.resolve("out"))

    from(project.rootDir.resolve("module"))
}

tasks.register<Zip>("zip-resetprop") {
    dependsOn("copyFiles")

    archiveFileName.set("PlayIntegrityFix-resetprop.zip")
    destinationDirectory.set(project.rootDir.resolve("out"))

    from(project.rootDir.resolve("module_resetprop"))
}

afterEvaluate {
    tasks["assembleRelease"].finalizedBy("copyFiles", "zip", "copyFiles-resetprop", "zip-resetprop")
}