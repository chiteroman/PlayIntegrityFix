APP_ABI      := armeabi-v7a arm64-v8a x86 x86_64
APP_CFLAGS   := -fvisibility=hidden -fvisibility-inlines-hidden -ffunction-sections -fdata-sections -Oz -flto
APP_CPPFLAGS := -std=c++20 -fno-exceptions -fno-rtti
APP_LDFLAGS  := -Oz -flto -Wl,--exclude-libs,ALL -Wl,--gc-sections
APP_STL      := none
APP_PLATFORM := android-26