
	STRIP ?= strip

	# check on which OS we build
	UNAME_S := $(shell uname -s)

# get flags supported by CPU
ifeq ($(UNAME_S), FreeBSD)
  CPU_INFO = dmesg | grep Features | tr A-Z a-z
  ifeq ($(UNAME_M), amd64)
    UNAME_M = x86_64
  endif
else
  CPU_INFO = cat /proc/cpuinfo | grep flags
endif

ifeq ($(MAKECMDGOALS),debug)
  CXXFLAGS += -g -DDEBUG
else
  CXXFLAGS += -O2 -DNDEBUG
endif

	# check CPU and supported optimization flags
	ifneq ($$(filter $(CPU_INFO) | grep sse3 ) , )
		SSE_CFLAGS = -msse3 -mfpmath=sse
	else ifneq ($$(filter $(CPU_INFO) | grep sse2 ) , )
		SSE_CFLAGS = -msse2 -mfpmath=sse
	else ifneq ($$(filter $(CPU_INFO) | grep sse ) , )
		SSE_CFLAGS = -msse -mfpmath=sse
	else ifneq ($$(filter $(CPU_INFO) | grep ARM ) , )
		ifneq ($$$(filter $(CPU_INFO)  | grep ARMv7 ) , )
			ifneq ($$(filter $(CPU_INFO) | grep vfpd32 ) , )
				SSE_CFLAGS = -march=armv7-a -mfpu=vfpv3
			else ifneq ($$(filter $(CPU_INFO) | grep vfpv3 ) , )
				SSE_CFLAGS = -march=armv7-a -mfpu=vfpv3
			endif
		else
			ARMCPU = "YES"
		endif
	else
		SSE_CFLAGS =
	endif

	# set bundle name
	NAME = loopino
	VER = 0.1

	PREFIX ?= /usr
	BIN_DIR ?= $(PREFIX)/bin
	SHARE_DIR ?= $(PREFIX)/share
	DESKAPPS_DIR ?= $(SHARE_DIR)/applications
	PIXMAPS_DIR ?= $(SHARE_DIR)/pixmaps

	LIB_DIR := ../libxputty/libxputty/
	HEADER_DIR := $(LIB_DIR)include/
	RESOURCES := $(wildcard $(RESOURCES_DIR)*.png)

	RESAMP_DIR := ./zita-resampler-1.1.0/
	RESAMP_SOURCES := $(wildcard $(RESAMP_DIR)*.cc)
	RESAMP_OBJ := $(patsubst %.cc,%.o,$(RESAMP_SOURCES))
	RESAMP_LIB := libzita-resampler.$(STATIC_LIB_EXT)

ifeq ($(TARGET), Linux)
	# set compile flags
	CFLAGS += -I. -I./zita-resampler-1.1.0 -Wall -funroll-loops `$(PKGCONFIG) $(PKGCONFIG_FLAGS) --cflags sndfile jack $(USEAPI) fftw3f`\
	-ffast-math -fomit-frame-pointer -fstrength-reduce -fdata-sections -Wl,--gc-sections \
	-pthread $(SSE_CFLAGS)
	CXXFLAGS += -MMD -std=c++20 -D_OS_UNIX_ -DALVER=\"$(VER)\" $(CFLAGS)
	LDFLAGS += -I. -lm -pthread -lpthread `$(PKGCONFIG) $(PKGCONFIG_FLAGS) --libs jack sndfile $(USEAPI) fftw3f`
	ifneq ($(MACOS)$(WINDOWS),true)
		LDFLAGS += -lrt -lc
	endif
	GUI_LDFLAGS += -I. -I$(HEADER_DIR) \
	-L. $(LIB_DIR)libxputty.a  `$(PKGCONFIG) $(PKGCONFIG_FLAGS) --cflags --libs cairo x11` -lm
else ifeq ($(TARGET), Windows)

	CXXFLAGS += -MMD -std=c++20 -DALVER=\"$(VER)\" $(CFLAGS)

	LDFLAGS += -I. -lm -pthread -lpthread  $(PAWPAW_LFLAGS) `$(PKGCONFIG) --cflags --libs jack sndfile fftw3f`

	GUI_LDFLAGS += -I$(HEADER_DIR) -static-libgcc -static-libstdc++ \
	`$(PKGCONFIG) $(PKGCONFIG_FLAGS) --cflags --libs cairo ` \
	-L. $(LIB_DIR)libxputty.$(STATIC_LIB_EXT) -lm $(PAWPAW_LFLAGS)

	CFLAGS += -I. -I./zita-resampler-1.1.0 -Wall -funroll-loops `$(PKGCONFIG) --cflags jack sndfile fftw3f`\
	-ffast-math -fomit-frame-pointer -fstrength-reduce -fdata-sections -Wl,--gc-sections \
	-pthread $(SSE_CFLAGS)

	EXE := .exe
endif


	# invoke build files
	OBJECTS = main.cpp

	DEPS = loopino.d $(RESAMP_DIR)resampler.d  $(RESAMP_DIR)resampler_table.d

.PHONY : mod all clean install uninstall

all : check $(NAME)
	$(QUIET)mkdir -p ../bin
	$(QUIET)cp ./$(NAME)$(EXE) ../bin
	@if ! [ -f ../bin/$(NAME)$(EXE) ]; then \
		$(R_ECHO) "Sorry, build fail$(reset)"; \
	fi

debug : all


-include $(DEPS)

check :
ifdef ARMCPU
	@$(R_ECHO)"ARM CPU DEDECTED, please check the optimization flags$(reset)"
endif


clean :
	$(QUIET)rm -f *.o *.d *.a *.lib 
	$(QUIET)rm -f $(RESAMP_DIR)*.a $(RESAMP_DIR)*.lib $(RESAMP_DIR)*.o $(RESAMP_DIR)*.d
	$(QUIET)rm -f $(NAME).exe $(NAME)
	$(QUIET)rm -rf ../bin

dist-clean :

install :
	$(QUIET)mkdir -p $(DESTDIR)$(BIN_DIR)
	$(QUIET)cp ./$(BUILD_DIR)/$(NAME) $(DESTDIR)$(BIN_DIR)/$(NAME)
	$(QUIET)mkdir -p $(DESTDIR)$(DESKAPPS_DIR)
	$(QUIET)cp $(NAME).desktop $(DESTDIR)$(DESKAPPS_DIR)
	$(QUIET)cp $(NAME).svg $(DESTDIR)$(PIXMAPS_DIR)
	$(QUIET)update-desktop-database || true

uninstall :
	$(QUIET)rm -rf $(DESTDIR)$(BIN_DIR)/$(NAME)
	$(QUIET)rm -rf $(DESTDIR)$(DESKAPPS_DIR)/$(NAME).desktop
	$(QUIET)rm -rf $(DESTDIR)$(PIXMAPS_DIR)/$(NAME).svg

$(RESAMP_OBJ): $(RESAMP_SOURCES)
	@$(ECHO) "Building object file $@ $(reset)"
	$(QUIET)$(CXX) $(CXXFLAGS) -MMD  -c $(patsubst %.o,%.cc,$@) -o $@ -I./zita-resampler-1.1.0

$(RESAMP_LIB): $(RESAMP_OBJ)
	@$(B_ECHO) "Build static library $@ $(reset)"
	$(QUIET)$(AR) rcs $(RESAMP_LIB) $(RESAMP_OBJ)
	@$(B_ECHO) "=================== DONE =======================$(reset)"

$(NAME) : $(OBJECTS) $(RESAMP_LIB) Loopino_ui.h
	@$(B_ECHO) "Build $@ $(reset)"
	$(QUIET)$(CXX) -MMD $(CXXFLAGS) $(OBJECTS) -L. $(RESAMP_LIB) -o $(NAME)$(EXE) $(LDFLAGS) $(GUI_LDFLAGS)
ifneq ($(MAKECMDGOALS),debug)
	$(QUIET)$(STRIP) -s -x -X -R .note.ABI-tag $(NAME)$(EXE)
endif
	@$(B_ECHO) "=================== DONE =======================$(reset)"

doc:
	#pass
