APP_STL      := none
APP_CFLAGS   := -fvisibility=hidden -fvisibility-inlines-hidden -O3 -mllvm -polly
APP_CPPFLAGS := -std=c++20 -fno-exceptions -fno-rtti
APP_LDFLAGS  := -O3 -mllvm -polly