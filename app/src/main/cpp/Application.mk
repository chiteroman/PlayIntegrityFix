APP_STL      := none
APP_ABI      := arm64-v8a armeabi-v7a
APP_CFLAGS   := -Oz -flto -fvisibility=hidden -fvisibility-inlines-hidden
APP_CPPFLAGS := -std=c++20 -fno-exceptions -fno-rtti
APP_LDFLAGS  := -Oz -flto