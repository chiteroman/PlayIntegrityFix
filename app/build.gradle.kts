plugins {
    id("com.android.application")
}

android {
    namespace = "es.chiteroman.playintegrityfix"
    compileSdk = 34
    ndkVersion = "26.1.10909125"
    buildToolsVersion = "34.0.0"

    defaultConfig {
        applicationId = "es.chiteroman.playintegrityfix"
        minSdk = 26
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"

        packaging {
            jniLibs {
                excludes += "**/libdobby.so"
            }
        }

        buildFeatures {
            prefab = true
        }

        externalNativeBuild {
            cmake {
                arguments += setOf("-DANDROID_STL=none", "-DCMAKE_BUILD_TYPE=MinSizeRel")
                cFlags += setOf("-flto=full", "-fvisibility=hidden", "-fvisibility-inlines-hidden", "-ffunction-sections", "-fdata-sections")
                cppFlags += setOf("-std=c++20", "-fno-exceptions", "-fno-rtti", "-flto=full", "-fvisibility=hidden", "-fvisibility-inlines-hidden", "-ffunction-sections", "-fdata-sections")
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