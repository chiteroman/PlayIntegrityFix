plugins {
    alias(libs.plugins.android.application)
}

android {
    namespace = "es.chiteroman.playintegrityfix"
    compileSdk = 35

    buildFeatures {
        prefab = true
    }

    packaging {
        resources {
            excludes += "**"
        }
    }

    defaultConfig {
        minSdk = 26
        multiDexEnabled = false

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
            multiDexEnabled = false
            proguardFiles += file("proguard-rules.pro")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_21
        targetCompatibility = JavaVersion.VERSION_21
    }

    externalNativeBuild {
        cmake {
            path("src/main/cpp/CMakeLists.txt")
        }
    }
}

dependencies {
    implementation(libs.cxx)
    implementation(libs.hiddenapibypass)
}

afterEvaluate {
    tasks.named("assembleRelease") {
        finalizedBy(
            rootProject.tasks["copyZygiskFiles"],
            rootProject.tasks["zip"]
        )
    }
}