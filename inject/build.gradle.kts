plugins {
    alias(libs.plugins.android.library)
}

android {
    namespace = "es.chiteroman.inject"
    compileSdk = 35

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
                    "-DCMAKE_BUILD_TYPE=MinSizeRel",
                    "-DANDROID_STL=none"
                )

                cFlags(
                    "-std=c23",
                    "-fvisibility=hidden",
                    "-fvisibility-inlines-hidden"
                )

                cppFlags(
                    "-std=c++26",
                    "-fno-exceptions",
                    "-fno-rtti",
                    "-fvisibility=hidden",
                    "-fvisibility-inlines-hidden"
                )
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
        }
    }
}

dependencies {
    implementation(libs.cxx)
}

afterEvaluate {
    tasks.named("assembleRelease") {
        finalizedBy(
            rootProject.tasks["copyInjectFiles"],
            rootProject.tasks["zip"]
        )
    }
}