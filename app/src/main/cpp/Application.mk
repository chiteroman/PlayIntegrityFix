APP_STL          := system
APP_ABI          := armeabi-v7a arm64-v8a
APP_CFLAGS       := -Oz -flto -fvisibility=hidden -fvisibility-inlines-hidden -ffunction-sections -fdata-sections -fno-threadsafe-statics -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector
APP_CPPFLAGS     := -std=c++2b -fno-exceptions -fno-rtti
APP_CONLYFLAGS   := -std=c2x
APP_LDFLAGS      := -Oz -flto -Wl,--exclude-libs,ALL -Wl,--gc-sections
APP_PLATFORM     := android-26
APP_THIN_ARCHIVE := true