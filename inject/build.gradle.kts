plugins {
    alias(libs.plugins.android.library)
}

android {
    namespace = "es.chiteroman.inject"
    compileSdk = 35
    buildToolsVersion = "35.0.1"
    ndkVersion = "28.0.13004108"

    buildFeatures {
        prefab = true
    }

    packaging {
        jniLibs {
            excludes += "**/libdobby.so"
        }
        resources {
            excludes += "**"
        }
    }

    defaultConfig {
        minSdk = 26

        externalNativeBuild {
            cmake {
                abiFilters(
                    "arm64-v8a",
                    "armeabi-v7a"
                )

                arguments(
                    "-DCMAKE_BUILD_TYPE=Release",
                    "-DANDROID_STL=c++_static",
                    "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON",
                    "-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON"
                )

                cFlags("-std=c23")

                cppFlags("-std=c++2c")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro"
            )
        }
    }

    externalNativeBuild {
        cmake {
            path("src/main/cpp/CMakeLists.txt")
            version = "3.28.0+"
        }
    }
}

afterEvaluate {
    tasks.named("assembleRelease") {
        finalizedBy(
            rootProject.tasks["copyInjectFiles"],
            rootProject.tasks["zip"]
        )
    }
}
