plugins {
    id("com.android.application")
}

android {
    namespace = "es.chiteroman.playintegrityfix"
    compileSdk = 34
    ndkVersion = "26.2.11394342"
    buildToolsVersion = "34.0.0"

    defaultConfig {
        applicationId = "es.chiteroman.playintegrityfix"
        minSdk = 26
        targetSdk = 34
        versionCode = 15920
        versionName = "v15.9.2"
        multiDexEnabled = false

        buildFeatures {
            prefab = true
        }

        packaging {
            jniLibs {
                excludes += "**/liblog.so"
                excludes += "**/libdobby.so"
                excludes += "**/libshadowhook.so"
            }
        }

        externalNativeBuild {
            cmake {
                abiFilters += "arm64-v8a"
                abiFilters += "armeabi-v7a"

                arguments += "-DANDROID_STL=none"
                arguments += "-DCMAKE_BUILD_TYPE=MinSizeRel"

                cFlags += "-std=c2x"
                cFlags += "-fvisibility=hidden"
                cFlags += "-fvisibility-inlines-hidden"
                cFlags += "-flto"

                cppFlags += "-std=c++2b"
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
            multiDexEnabled = false
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
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

tasks.register("updateModuleProp") {
    doLast {
        val versionName = project.android.defaultConfig.versionName
        val versionCode = project.android.defaultConfig.versionCode

        val modulePropFile = project.rootDir.resolve("module/module.prop")

        var content = modulePropFile.readText()

        content = content.replace(Regex("version=.*"), "version=$versionName")
        content = content.replace(Regex("versionCode=.*"), "versionCode=$versionCode")

        modulePropFile.writeText(content)
    }
}


tasks.register("copyFiles") {
    dependsOn("updateModuleProp")

    doLast {
        val moduleFolder = project.rootDir.resolve("module")
        val dexFile = project.layout.buildDirectory.get().asFile.resolve("intermediates/dex/release/minifyReleaseWithR8/classes.dex")
        val soDir = project.layout.buildDirectory.get().asFile.resolve("intermediates/stripped_native_libs/release/out/lib")

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

    archiveFileName.set("PlayIntegrityFix_${project.android.defaultConfig.versionName}.zip")
    destinationDirectory.set(project.rootDir.resolve("out"))

    from(project.rootDir.resolve("module"))
}

afterEvaluate {
    tasks["assembleRelease"].finalizedBy("updateModuleProp", "copyFiles", "zip")
}