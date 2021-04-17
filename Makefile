CORE_DIR    := .
INCFLAGS    :=
SOURCES_C   :=
SOURCES_CC  :=
SOURCES_CXX :=
OBJECTS     :=

ifeq ($(platform),)
platform = unix
ifeq ($(shell uname -a),)
   platform = win
else ifneq ($(findstring MINGW,$(shell uname -a)),)
   platform = win
else ifneq ($(findstring Darwin,$(shell uname -a)),)
   platform = osx
else ifneq ($(findstring win,$(shell uname -a)),)
   platform = win
endif
endif

ifeq ($(EMUTYPE),)
   EMUTYPE = x64
endif

TARGET_NAME := vice_$(EMUTYPE)

# system platform
system_platform = unix
ifeq ($(shell uname -a),)
   EXE_EXT = .exe
   system_platform = win
else ifneq ($(findstring Darwin,$(shell uname -a)),)
   system_platform = osx
   arch = intel
   ifeq ($(shell uname -p),powerpc)
	arch = ppc
   endif
   ifeq ($(shell uname -p),arm)
	arch = arm
   endif
else ifneq ($(findstring MINGW,$(shell uname -a)),)
   system_platform = win
endif

# Unix
ifeq ($(platform), unix)
   TARGET := $(TARGET_NAME)_libretro.so
   LDFLAGS += -shared -Wl,--version-script=$(CORE_DIR)/libretro/link.T -Wl,--gc-sections
   fpic = -fPIC

# CrossPI
else ifeq ($(platform), crosspi)
   CC = ~/RPI/tools-master/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin/arm-linux-gnueabihf-gcc
   CXX =~/RPI/tools-master/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin/arm-linux-gnueabihf-g++
   TARGET := $(TARGET_NAME)_libretro.so
   fpic := -fPIC
   SHARED := -L~/RPI/usr/lib/arm-linux-gnueabihf -L~/RPI/lib -L~/RPI/lib/arm-linux-gnueabihf -shared -Wl,--no-undefined
   CFLAGS += -DARM -DRPIPORT -DALIGN_DWORD -mstructure-size-boundary=32 -mthumb-interwork -falign-functions=16 -marm
   CFLAGS += -march=armv7-a -mfloat-abi=hard -mfpu=vfpv3 -O2 -pipe -fstack-protector
   CFLAGS += -I~/RPI/usr/include/arm-linux-gnueabihf -I/home/tech/RPI/usr/include
   CXXFLAGS += $(CFLAGS)
   LDFLAGS += -shared -Wl,--version-script=$(CORE_DIR)/libretro/link.T -L/home/tech/RPI/usr/lib

# Classic Platforms ####################
# Platform affix = classic_<ISA>_<µARCH>

# (armv7 a7, hard point, neon based) ###
# NESC, SNESC, C64 mini
else ifeq ($(platform), classic_armv7_a7)
   TARGET := $(TARGET_NAME)_libretro.so
   fpic := -fPIC
   LDFLAGS := -shared -Wl,--version-script=$(CORE_DIR)/libretro/link.T -Wl,--no-undefined
   CFLAGS += -DARM -Ofast \
	-flto=4 -fwhole-program -fuse-linker-plugin \
	-fdata-sections -ffunction-sections -Wl,--gc-sections \
	-fno-stack-protector -fno-ident -fomit-frame-pointer \
	-falign-functions=1 -falign-jumps=1 -falign-loops=1 \
	-fno-unwind-tables -fno-asynchronous-unwind-tables -fno-unroll-loops \
	-fmerge-all-constants -fno-math-errno \
	-marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard
   CXXFLAGS += $(CFLAGS)
   CPPFLAGS += $(CFLAGS)
   ASFLAGS += $(CFLAGS)
   ifeq ($(shell echo `$(CC) -dumpversion` "< 4.9" | bc -l), 1)
      CFLAGS += -march=armv7-a
   else
      CFLAGS += -march=armv7ve
	  # If gcc is 5.0 or later
	  ifeq ($(shell echo `$(CC) -dumpversion` ">= 5" | bc -l), 1)
	     LDFLAGS += -static-libgcc -static-libstdc++
      endif
   endif

# (armv8 a35, hard point, neon based) ###
# PS Classic
else ifeq ($(platform), classic_armv8_a35)
   TARGET := $(TARGET_NAME)_libretro.so
   fpic := -fPIC
   LDFLAGS := -shared -Wl,--version-script=$(CORE_DIR)/libretro/link.T -Wl,--no-undefined
   CFLAGS += -DARM -Ofast \
	-flto=4 -fwhole-program -fuse-linker-plugin \
	-fdata-sections -ffunction-sections -Wl,--gc-sections \
	-fno-stack-protector -fno-ident -fomit-frame-pointer \
	-falign-functions=1 -falign-jumps=1 -falign-loops=1 \
	-fno-unwind-tables -fno-asynchronous-unwind-tables -fno-unroll-loops \
	-fmerge-all-constants -fno-math-errno \
	-marm -mtune=cortex-a35 -mfpu=neon-fp-armv8 -mfloat-abi=hard
   CXXFLAGS += $(CFLAGS)
   CPPFLAGS += $(CFLAGS)
   ASFLAGS += $(CFLAGS)
   CFLAGS += -march=armv8-a
   LDFLAGS += -static-libgcc -static-libstdc++
#######################################

# CTR (3DS)
else ifeq ($(platform), ctr)
   TARGET := $(TARGET_NAME)_libretro_$(platform).a
   CC = $(DEVKITARM)/bin/arm-none-eabi-gcc$(EXE_EXT)
   CXX = $(DEVKITARM)/bin/arm-none-eabi-g++$(EXE_EXT)
   AR = $(DEVKITARM)/bin/arm-none-eabi-ar$(EXE_EXT)
   DEFINES += -D_3DS -DARM11 -march=armv6k -mtune=mpcore -mfloat-abi=hard
   CFLAGS += $(DEFINES) -I$(CTRULIB)/include
   CXXFLAGS += $(CFLAGS)
   STATIC_LINKING = 1

# Nintendo Switch (libnx)
else ifeq ($(platform), libnx)
   include $(DEVKITPRO)/libnx/switch_rules
   TARGET := $(TARGET_NAME)_libretro_$(platform).a
   CFLAGS += -O3 -fomit-frame-pointer -ffast-math -I$(DEVKITPRO)/libnx/include/ -Wl,--allow-multiple-definition
   CFLAGS += -specs=$(DEVKITPRO)/libnx/switch.specs
   CFLAGS += -D__SWITCH__ -DHAVE_LIBNX -DHAVE_GETPWUID=0 -DHAVE_GETCWD=1
   CFLAGS += -march=armv8-a -mtune=cortex-a57 -mtp=soft -ffast-math -mcpu=cortex-a57+crc+fp+simd -ffunction-sections -fcommon
   CFLAGS += -Ifrontend/switch -ftree-vectorize
   CXXFLAGS += $(CFLAGS)
   fpic = -fPIE
   STATIC_LINKING=1

# OSX
else ifeq ($(platform), osx)
   TARGET := $(TARGET_NAME)_libretro.dylib
   LDFLAGS += -dynamiclib
   fpic = -fPIC
   MINVERSION :=
   ifeq ($(arch),ppc)
      COMMONFLAGS += -DBLARGG_BIG_ENDIAN=1 -D__ppc__
   endif
   OSXVER = `sw_vers -productVersion | cut -d. -f 2`
   OSX_LT_MAVERICKS = `(( $(OSXVER) <= 9)) && echo "YES"`
ifeq ($(OSX_LT_MAVERICKS),YES)
   MINVERSION = -mmacosx-version-min=10.1
else
   fpic += -stdlib=libc++
endif
   fpic += $(MINVERSION)
   CFLAGS += -DHAVE_STRLCPY -DHAVE_VSNPRINTF -DHAVE_SNPRINTF -DHAVE_STPCPY -D_INTTYPES_H
   CXXFLAGS += -DHAVE_STRLCPY -DHAVE_VSNPRINTF -DHAVE_SNPRINTF -DHAVE_STPCPY -D_INTTYPES_H
   ifeq ($(CROSS_COMPILE),1)
	TARGET_RULE   = -target $(LIBRETRO_APPLE_PLATFORM) -isysroot $(LIBRETRO_APPLE_ISYSROOT)
	CFLAGS     += $(TARGET_RULE)
	CXXFLAGS   += $(TARGET_RULE)
	LDFLAGS    += $(TARGET_RULE)
   endif

# iOS
else ifneq (,$(findstring ios,$(platform)))
   TARGET := $(TARGET_NAME)_libretro_ios.dylib
   COMMONFLAGS += -DHAVE_POSIX_MEMALIGN=1 -marm
   MINVERSION :=
   fpic = -fPIC
   LDFLAGS += -dynamiclib
   ifeq ($(IOSSDK),)
      IOSSDK := $(shell xcodebuild -version -sdk iphoneos Path)
   endif
   ifeq ($(platform), ios-arm64)
      CC = clang -arch arm64 -isysroot $(IOSSDK)
      CXX = clang++ -arch arm64 -isysroot $(IOSSDK)
   else
      CC = clang -arch armv7 -isysroot $(IOSSDK)
      CXX = clang++ -arch armv7 -isysroot $(IOSSDK)
   endif
   COMMONFLAGS += -DIOS
   CFLAGS += -DHAVE_STRLCPY -DHAVE_VSNPRINTF -DHAVE_SNPRINTF -DHAVE_STPCPY -D_INTTYPES_H
   CXXFLAGS += -DHAVE_STRLCPY -DHAVE_VSNPRINTF -DHAVE_SNPRINTF -DHAVE_STPCPY -D_INTTYPES_H
   OSXVER = `sw_vers -productVersion | cut -d. -f 2`
   OSX_LT_MAVERICKS = `(( $(OSXVER) <= 9)) && echo "YES"`
   ifeq ($(OSX_LT_MAVERICKS),"YES")
      MINVERSION = -miphoneos-version-min=5.0
   endif
   COMMONFLAGS += $(MINVERSION)

else ifeq ($(platform), tvos-arm64)
   TARGET := $(TARGET_NAME)_libretro_tvos.dylib
   COMMONFLAGS += -DHAVE_POSIX_MEMALIGN=1 -marm
   fpic = -fPIC
   LDFLAGS += -dynamiclib
   ifeq ($(IOSSDK),)
      IOSSDK := $(shell xcodebuild -version -sdk appletvos Path)
   endif
   COMMONFLAGS += -DIOS -DHAVE_STRLCPY -DHAVE_VSNPRINTF -DHAVE_SNPRINTF -DHAVE_STPCPY -D_INTTYPES_H
   CFLAGS   += $(COMMONFLAGS)
   CXXFLAGS += $(COMMONFLAGS)
   CC = clang -arch arm64 -isysroot $(IOSSDK)
   CXX = clang++ -arch arm64 -isysroot $(IOSSDK)

else ifeq ($(platform), theos_ios)
   DEPLOYMENT_IOSVERSION = 5.0
   TARGET = iphone:latest:$(DEPLOYMENT_IOSVERSION)
   ARCHS = armv7 armv7s
   TARGET_IPHONEOS_DEPLOYMENT_VERSION=$(DEPLOYMENT_IOSVERSION)
   THEOS_BUILD_DIR := objs
   COMMONFLAGS += -DIOS
   COMMONFLAGS += -DHAVE_POSIX_MEMALIGN=1 -marm
   include $(THEOS)/makefiles/common.mk
   LIBRARY_NAME = $(TARGET_NAME)_libretro_ios

# Blackberry
else ifeq ($(platform), qnx)
   TARGET := $(TARGET_NAME)_libretro_qnx.so
   fpic := -fPIC
   SHARED := -lcpp -lm -shared -Wl,-version-script=link.T
   CC = qcc -Vgcc_ntoarmv7le
   CC_AS = qcc -Vgcc_ntoarmv7le
   CXX = QCC -Vgcc_ntoarmv7le_cpp
   AR = QCC -Vgcc_ntoarmv7le
   COMMONFLAGS += -D__BLACKBERRY_QNX__ -fexceptions -marm -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=softfp
   CFLAGS += -std=gnu99

# ANDROID STANDALONE TOOLCHAIN
else ifeq ($(platform), androidstc)
   TARGET := $(TARGET_NAME)_libretro_android.so
   CC = arm-linux-androideabi-gcc
   CXX = arm-linux-androideabi-g++
   LDFLAGS += -lstdc++ -llog -shared -Wl,--version-script=$(CORE_DIR)/libretro/link.T -Wl,--no-undefined
   CFLAGS += -DHAVE_GETCWD=1 -DHAVE_MEMMOVE=1 -DHAVE_ATEXIT=1 -DARM -DALIGN_DWORD -DANDROID -mstructure-size-boundary=32 -mthumb-interwork -falign-functions=16 -marm
   CFLAGS += -O2 -pipe -fstack-protector
   CXXFLAGS += $(CFLAGS)
   fpic = -fPIC

# ANDROID
else ifeq ($(platform), android)
   ifeq ($(ANDROID_NDK_ARM),)
      $(error ANDROID_NDK_ARM not set correctly.)
   endif
   ifeq ($(ANDROID_NDK_ROOT),)
      $(error ANDROID_NDK_ROOT not set correctly.)
   endif

   TARGET := $(TARGET_NAME)_libretro_android.so

   CC = $(ANDROID_NDK_ARM)/bin/arm-linux-androideabi-gcc
   CXX = $(ANDROID_NDK_ARM)/bin/arm-linux-androideabi-g++

   #CFLAGS += -fPIC -fpic -ffunction-sections -funwind-tables
   CFLAGS += -DHAVE_GETCWD=1 -DHAVE_MEMMOVE=1 -DHAVE_ATEXIT=1 -DARM -DALIGN_DWORD

   COMMONFLAGS += -march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16 -mthumb -DANDROID -DALIGN_INTS -DALIGN_SHORTS
   COMMONFLAGS += -I$(ANDROID_NDK_ROOT)/platforms/android-19/arch-arm/usr/include -I$(ANDROID_NDK_ROOT)/sources/cxx-stl/gnu-libstdc++/4.9/include
   COMMONFLAGS += -I$(ANDROID_NDK_ROOT)/sources/cxx-stl/gnu-libstdc++/4.9/libs/armeabi-v7a/include

   LDFLAGS += -llog -shared -Wl,--version-script=$(CORE_DIR)/libretro/link.T -Wl,--no-undefined
   LDFLAGS += $(fpic) $(SHARED) -L$(ANDROID_NDK_ROOT)/sources/cxx-stl/gnu-libstdc++/4.9/libs/armeabi-v7a/thumb
   LDFLAGS += -L$(ANDROID_NDK_ROOT)/platforms/android-19/arch-arm/usr/lib  --sysroot=$(ANDROID_NDK_ROOT)/platforms/android-19/arch-arm -march=armv7-a -mthumb -shared
   LDFLAGS += -lc -ldl -lm -landroid -llog -lsupc++ $(ANDROID_NDK_ROOT)/sources/cxx-stl/gnu-libstdc++/4.9/libs/armeabi-v7a/thumb/libgnustl_static.a -lgcc

   CXXFLAGS += $(CFLAGS)
   fpic = -fPIC

# PSP
else ifeq ($(platform), psp1)
   TARGET := $(TARGET_NAME)_libretro_psp1.a
   CC = psp-gcc$(EXE_EXT)
   CXX = psp-g++$(EXE_EXT)
   AR = psp-ar$(EXE_EXT)
   COMMONFLAGS += -DPSP -G0 -I$(shell psp-config --pspsdk-path)/include
   CFLAGS += -std=c99
   CXXFLAGS += -std=c99
   STATIC_LINKING = 1

# Vita
else ifeq ($(platform), vita)
   TARGET := $(TARGET_NAME)_libretro_vita.a
   CC = arm-vita-eabi-gcc$(EXE_EXT)
   CXX = arm-vita-eabi-g++$(EXE_EXT)
   AR = arm-vita-eabi-ar$(EXE_EXT)
   COMMONFLAGS += -U__INT32_TYPE__ -U __UINT32_TYPE__ -D__INT32_TYPE__=int
   COMMONFLAGS += -DHAVE_STRTOUL -DVITA -fcommon
   STATIC_LINKING = 1

# Emscripten
else ifeq ($(platform), emscripten)
   TARGET := $(TARGET_NAME)_libretro_emscripten.bc
   fpic := -fPIC
   SHARED := -shared -s TOTAL_MEMORY=67108864
   STATIC_LINKING = 1
   COMMONFLAGS += -DHAVE_TIME_T_IN_TIME_H

# Wii
else ifeq ($(platform), wii)
   TARGET := $(TARGET_NAME)_libretro_wii.a
   CC = $(DEVKITPPC)/bin/powerpc-eabi-gcc$(EXE_EXT)
   CXX = $(DEVKITPPC)/bin/powerpc-eabi-g++$(EXE_EXT)
   AR = $(DEVKITPPC)/bin/powerpc-eabi-ar$(EXE_EXT)
   COMMONFLAGS += -DGEKKO -mrvl -mcpu=750 -meabi -mhard-float
   STATIC_LINKING = 1

# WiiU
else ifeq ($(platform), wiiu)
   TARGET := $(TARGET_NAME)_libretro_wiiu.a
   CC = $(DEVKITPPC)/bin/powerpc-eabi-gcc$(EXE_EXT)
   CXX = $(DEVKITPPC)/bin/powerpc-eabi-g++$(EXE_EXT)
   AR = $(DEVKITPPC)/bin/powerpc-eabi-ar$(EXE_EXT)
   COMMONFLAGS += -DGEKKO -mwup -mcpu=750 -meabi -mhard-float
   COMMONFLAGS += -U__INT32_TYPE__ -U __UINT32_TYPE__ -D__INT32_TYPE__=int
   COMMONFLAGS += -DHAVE_STRTOUL -DWIIU
   STATIC_LINKING = 1

# Lightweight PS3 Homebrew SDK
else ifeq ($(platform), psl1ght)
   TARGET := $(TARGET_NAME)_libretro_$(platform).a
   CC = $(PS3DEV)/ppu/bin/ppu-gcc$(EXE_EXT)
   CXX = $(PS3DEV)/ppu/bin/ppu-g++$(EXE_EXT)
   CC_AS = $(PS3DEV)/ppu/bin/ppu-gcc$(EXE_EXT)
   AR = $(PS3DEV)/ppu/bin/ppu-ar$(EXE_EXT)
   OLD_GCC := 1
   COMMONFLAGS += -DHAVE_STRTOUL -D__PSL1GHT__ -D__PS3__
   STATIC_LINKING = 1

# GCW0
else ifeq ($(platform), gcw0)
   TARGET := $(TARGET_NAME)_libretro.so
   CC = /opt/gcw0-toolchain/usr/bin/mipsel-linux-gcc
   CXX = /opt/gcw0-toolchain/usr/bin/mipsel-linux-g++
   AR = /opt/gcw0-toolchain/usr/bin/mipsel-linux-ar
   LDFLAGS += -shared -Wl,--version-script=$(CORE_DIR)/libretro/link.T
   fpic := -fPIC
   OLD_GCC := 1
   COMMONFLAGS += -DDINGUX -fomit-frame-pointer -ffast-math -march=mips32 -mtune=mips32r2 -mhard-float
   COMMONFLAGS += -DHAVE_SYS_TYPES_H -D_BSD_SOURCE -D_POSIX_C_SOURCE=200809L
   CFLAGS += -std=c99

# ARM
else ifneq (,$(findstring armv,$(platform)))
   TARGET := $(TARGET_NAME)_libretro.so
   fpic := -fPIC
   SHARED := -shared -Wl,--no-undefined
   LDFLAGS += -shared -Wl,--version-script=$(CORE_DIR)/libretro/link.T
   ifneq (,$(findstring neon,$(platform)))
      CFLAGS += -mfpu=neon
      HAVE_NEON = 1
   endif
   ifneq (,$(findstring softfloat,$(platform)))
      CFLAGS += -mfloat-abi=softfp
   else ifneq (,$(findstring hardfloat,$(platform)))
      CFLAGS += -mfloat-abi=hard
   endif
   CFLAGS += -DARM -marm -DALIGN_DWORD -mstructure-size-boundary=32 -mthumb-interwork -falign-functions=16 -pipe -fstack-protector

# Wincross64
else ifeq ($(platform), wincross64)
   AR = x86_64-w64-mingw32-ar
   CC = x86_64-w64-mingw32-gcc
   CXX = x86_64-w64-mingw32-g++
   CFLAGS += -D__WIN32__
   TARGET := $(TARGET_NAME)_libretro.dll
   LDFLAGS += --shared -static-libgcc -static-libstdc++ -Wl,--version-script=$(CORE_DIR)/libretro/link.T -L/usr/x86_64-w64-mingw32/lib
   LDFLAGS += -lws2_32 -luser32 -lwinmm -ladvapi32 -lshlwapi -lwsock32 -lws2_32 -lpsapi -liphlpapi -lshell32 -luserenv -lmingw32 -shared -lgcc -lm -lmingw32

# Windows
else
   CFLAGS += -D__WIN32__
   TARGET := $(TARGET_NAME)_libretro.dll
   LDFLAGS += --shared -static-libgcc -static-libstdc++ -Wl,--version-script=$(CORE_DIR)/libretro/link.T -Wl,--gc-sections -L/usr/x86_64-w64-mingw32/lib
   LDFLAGS += -lws2_32 -luser32 -lwinmm -ladvapi32 -lshlwapi -lwsock32 -lws2_32 -lpsapi -liphlpapi -lshell32 -luserenv -lmingw32 -shared -lgcc -lm -lmingw32
endif

# Common
ifeq ($(DEBUG), 1)
   COMMONFLAGS += -O0 -g
else
   COMMONFLAGS += -O3 -DNDEBUG -Wno-format -Wno-format-security
   LDFLAGS     += -s
endif

# Skip 7zip for static builds
ifneq ($(STATIC_LINKING), 1)
   COMMONFLAGS += -DHAVE_7ZIP
endif

COMMONFLAGS += -DWANT_ZLIB -DHAVE_CONFIG_H -D__LIBRETRO__ -DCORE_NAME=\"$(EMUTYPE)\"

include Makefile.common

$(info CFLAGS: $(CFLAGS) $(COMMONFLAGS))
$(info -------)

OBJECTS     += $(patsubst %.cpp,%.o,$(SOURCES_CXX:.cc=.o)) $(SOURCES_C:.c=.o)
CXXFLAGS    += $(fpic) $(INCFLAGS) $(COMMONFLAGS)
CFLAGS      += $(fpic) $(INCFLAGS) $(COMMONFLAGS)
LDFLAGS     += -lm $(fpic)

# Ensure only a language version supported by all compilers is used
# Do not enforce C99 as some gcc-versions appear to not handle system-headers
# properly in that case.
#CFLAGS      += -std=c99
CXXFLAGS    += -std=c++98

ifeq ($(platform), theos_ios)
	COMMON_FLAGS := -DIOS -DARM $(COMMON_DEFINES) $(INCFLAGS) -I$(THEOS_INCLUDE_PATH) -Wno-error
	$(LIBRARY_NAME)_CFLAGS += $(CFLAGS) $(COMMON_FLAGS)
	$(LIBRARY_NAME)_CXXFLAGS += $(CXXFLAGS) $(COMMON_FLAGS)
	${LIBRARY_NAME}_FILES = $(SOURCES_CXX) $(SOURCES_C)
	include $(THEOS_MAKE_PATH)/library.mk
else
all: $(TARGET)
$(TARGET): $(OBJECTS)
ifeq ($(platform), emscripten)
	$(CXX) -r $(SHARED) -o $@ $(OBJECTS) $(LDFLAGS)
else  ifeq ($(STATIC_LINKING), 1)
	$(AR) rcs $@ $(OBJECTS)
else
	$(CXX) -o $@ $(OBJECTS) $(LDFLAGS)
endif

%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $^ -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: clean
endif
