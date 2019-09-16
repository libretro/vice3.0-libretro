APP_ABI := all
# Without the -Ofast here, the core crashes on armv7a devices when opening the on-screen keyboard with L.
# The crash is due to some unaligned memory access (BUS_ADRALN) and doesn't happen on other platforms.
# Building the core with the buildbot running NDK v20 on linux with default flags caused the crash.
# Building the core with NDK v20 on Darwin must use different default flags or do something else different
# because the crash didn't happen with those builds, and the resulting .so was also ~700 kb larger in size.
# This should be investigated more in the future.
APP_CFLAGS += -Wno-error=format-security -Ofast
