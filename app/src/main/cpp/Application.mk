APP_STL      := system
APP_CFLAGS   := -Oz -flto -fvisibility=hidden -fvisibility-inlines-hidden -faddrsig -ffunction-sections -fdata-sections
APP_CPPFLAGS := -std=c++20 -fno-exceptions -fno-rtti
APP_LDFLAGS  := -Oz -flto -Wl,--icf=all -Wl,--exclude-libs,ALL -Wl,--gc-sections