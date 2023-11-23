FLAGS := -DNDEBUG -g0 -Oz -fno-exceptions -fno-rtti -fvisibility=hidden -fvisibility-inlines-hidden -ffunction-sections -fdata-sections -flto=full -Wl,--icf=all -Wl,--exclude-libs,ALL -Wl,--gc-sections

APP_STL      := none
APP_CPPFLAGS := -std=c++20
APP_CFLAGS   := $(FLAGS)
APP_LDFLAGS  := $(FLAGS)