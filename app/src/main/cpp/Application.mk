APP_ABI      := armeabi-v7a arm64-v8a
APP_CFLAGS   := -DNDEBUG -Oz -fvisibility=hidden -fvisibility-inlines-hidden -ffunction-sections -fdata-sections
APP_CPPFLAGS := -std=c++20 -fno-exceptions -fno-rtti
APP_STL      := none
APP_PLATFORM := android-26