
	STRIP ?= strip

	# check on which OS we build
	UNAME_S := $(shell uname -s)
	# check which architecture we build on
	UNAME_M := $(shell uname -m)
	# check which architecture we build for
	TARGET_ARCH = $(shell $(CXX) -dumpmachine | sed 's/-.*//')

ifneq ($(UNAME_M),$(TARGET_ARCH))
  CROSS_COMPILING = 1
  $(info $(yellow) INFO: $(reset)Cross Compile $(blue)$(UNAME_M)$(reset) to $(blue)$(TARGET_ARCH)$(reset))
endif

# avoid optimisation for x86_64 arch when we cross compile
ifneq ($(CROSS_COMPILING), 1)

# get flags supported by CPU
ifeq ($(UNAME_S), FreeBSD)
  CPU_INFO = dmesg | grep Features | tr A-Z a-z
  ifeq ($(UNAME_M), amd64)
    UNAME_M = x86_64
  endif
else
  CPU_INFO = cat /proc/cpuinfo | grep flags
endif

# check for sse optimisation level only on x86_64 architecture
ifeq ($(TARGET_ARCH), x86_64)
  ifneq ($$(filter $(CPU_INFO) | grep sse3 ) , )
    SSE_CFLAGS = -msse3 -mfpmath=sse -mfxsr -DUSE_SSE=1
    FFT_FLAG = -DFFTCONVOLVER_USE_SSE=1
  else ifneq ($$(filter $(CPU_INFO) | grep sse2 ) , )
    SSE_CFLAGS = -msse2 -mfpmath=sse -mfxsr -DUSE_SSE=1
    FFT_FLAG = -DFFTCONVOLVER_USE_SSE=1
  else ifneq ($$(filter $(CPU_INFO) | grep sse ) , )
    SSE_CFLAGS = -msse -mfpmath=sse -mfxsr -DUSE_SSE=1
    FFT_FLAG = -DFFTCONVOLVER_USE_SSE=1
  else ifneq ($$(filter $(CPU_INFO) | grep ARM ) , )
    ifneq ($$(filter $(CPU_INFO) | grep ARMv7 ) , )
      ifneq ($$(filter $(CPU_INFO) | grep vfpd32 ) , )
        SSE_CFLAGS = -march=armv7-a -mfpu=vfpv3 
      else ifneq ($$(filter $(CPU_INFO) | grep vfpv3 ) , )
        SSE_CFLAGS = -march=armv7-a -mfpu=vfpv3
      endif
    else
      ARMCPU = "YES"
    endif
  endif
else
  SSE_CFLAGS =
endif

endif


ifeq ($(MAKECMDGOALS),debug)
  CXXFLAGS += -g -DDEBUG
else
  CXXFLAGS += -O3 -DNDEBUG
endif

ifneq ($(MAKECMDGOALS),clap)
  ifneq ($(MAKECMDGOALS),vst2)
    ifeq ($(TARGET), Linux)
      USEAPI = jack
      LDFLAGS += -lasound
    endif
  endif
endif

	# set bundle name
	NAME = loopino
	VER = 0.1

	PREFIX ?= /usr
	# check if user is root
	user = $(shell whoami)
	ifeq ($(user),root)
	INSTALL_DIR ?= $(PREFIX)/lib/lv2
	EXE_INSTALL_DIR ?= $(PREFIX)/bin
	CLAP_INSTAL_DIR ?= $(PREFIX)/lib/clap
	VST2_INSTAL_DIR ?= $(PREFIX)/lib/vst
	else 
	INSTALL_DIR ?= ~/.lv2
	EXE_INSTALL_DIR ?= ~/bin
	CLAP_INSTAL_DIR ?= ~/.clap
	VST2_INSTAL_DIR ?= ~/.vst
	endif

	SHARE_DIR ?= $(PREFIX)/share
	DESKAPPS_DIR ?= $(SHARE_DIR)/applications
	PIXMAPS_DIR ?= $(SHARE_DIR)/pixmaps

	LIB_DIR := ../libxputty/libxputty/
	HEADER_DIR := $(LIB_DIR)include/
	RESOURCES := $(wildcard $(RESOURCES_DIR)*.png)

	INCLUDES := -I. -I./Machines/ -I./Filter/ -I./Backends/ -I./GUI/

	CLAP_DIR := ./Clap/
	CLAP_INCLUDE := -I. -I./Clap/clap/ -I./Clap/
	CLAP_SOURCES := $(CLAP_DIR)ClapPlug.cpp

	VST2_DIR := ./vst2/
	VST2_INCLUDE := -I. -I./vst2/ -I./Clap/
	VST2_SOURCES := $(VST2_DIR)Loopinovst.cpp

ifeq ($(TARGET), Linux)
	# set compile flags
	CFLAGS += -Wall -funroll-loops `$(PKGCONFIG) $(PKGCONFIG_FLAGS) --cflags sndfile $(USEAPI) fftw3f `\
	-ffast-math -fomit-frame-pointer -fstrength-reduce -fdata-sections \
	-pthread $(SSE_CFLAGS)
	CXXFLAGS += -MMD -std=c++17 -fno-math-errno -fno-trapping-math -fPIC -D_OS_UNIX_ -DALVER=\"$(VER)\" $(CFLAGS)
	LDFLAGS += $(INCLUDES) -lm -pthread -lpthread -lstdc++ \
	-I../rubberband/ -I../rubberband/rubberband/ -L. ../rubberband/lib/librubberband.$(STATIC_LIB_EXT) \
	`$(PKGCONFIG) $(PKGCONFIG_FLAGS) --libs sndfile $(USEAPI) fftw3f `
	ifneq ($(MACOS)$(WINDOWS),true)
		LDFLAGS += -lrt -lc
	endif
	ifneq (,$(findstring clang, $(CXX)))
		CXXFLAGS := $(filter-out -fstrength-reduce,$(CXXFLAGS))
		CXXFLAGS += -Wno-unused-private-field
	endif
	GUI_LDFLAGS += -I. -I$(HEADER_DIR) -Wl,--gc-sections \
	-L. $(LIB_DIR)libxputty.a  `$(PKGCONFIG) $(PKGCONFIG_FLAGS) --cflags --libs cairo x11` -lm
else ifeq ($(TARGET), Windows)

	CXXFLAGS += -MMD -std=c++17 -fPIC -DALVER=\"$(VER)\" $(CFLAGS)

	LDFLAGS += $(INCLUDES) -lm -pthread -lpthread  $(PAWPAW_LFLAGS) \
	`$(PKGCONFIG) --cflags --libs $(USEAPI) sndfile fftw3f ` \
	-I../rubberband/ -I../rubberband/rubberband/ -L. ../rubberband/lib/librubberband.$(STATIC_LIB_EXT)

	GUI_LDFLAGS += -I$(HEADER_DIR) -static-libgcc -static-libstdc++ \
	`$(PKGCONFIG) $(PKGCONFIG_FLAGS) --cflags --libs cairo ` \
	-L. $(LIB_DIR)libxputty.$(STATIC_LIB_EXT) -lm $(PAWPAW_LFLAGS)

	CFLAGS += -Wall -funroll-loops `$(PKGCONFIG) --cflags $(USEAPI) sndfile fftw3f`\
	-ffast-math -fomit-frame-pointer -fstrength-reduce -fdata-sections -Wl,--gc-sections \
	-pthread $(SSE_CFLAGS)

	EXE := .exe
endif


	# invoke build files
	OBJECTS = main.cpp

	DEPS = loopino.d

.PHONY : mod all clean install uninstall info

ifeq ($(TARGET), Linux)
all : check $(NAME)
	$(QUIET)mkdir -p ../bin
	$(QUIET)cp ./$(NAME)$(EXE) ../bin
	@if ! [ -f ../bin/$(NAME)$(EXE) ]; then \
		$(R_ECHO) "Sorry, build fail$(reset)"; \
	fi
else
all: clap
endif

debug : all


clap: $(NAME).clap
	$(QUIET)mkdir -p ../bin/
	$(QUIET)cp ./$(NAME).clap ../bin/
	@$(B_ECHO) "=================== DONE =======================$(reset)"

vst2: $(NAME)vst.$(LIB_EXT)
	$(QUIET)mkdir -p ../bin/
	$(QUIET)cp ./$(NAME)vst.$(LIB_EXT) ../bin/
	@$(B_ECHO) "=================== DONE =======================$(reset)"

-include $(DEPS)

check :
ifdef ARMCPU
	@$(R_ECHO)"ARM CPU DEDECTED, please check the optimization flags$(reset)"
endif

info:
	$(info $(yellow) INFO: $(reset) cxxflags $(blue) $(CXX) $(CXXFLAGS) $(reset))
	$(info $(yellow) INFO: $(reset) ldflags  $(blue) $(LD) $(LDFLAGS) $(GUI_LDFLAGS))
	@$(B_ECHO) "=================== INFO =======================$(reset)"


clean :
	$(QUIET)rm -f *.o *.d *.a *.lib 
	$(QUIET)rm -f $(NAME).exe $(NAME) $(NAME).clap $(NAME)vst.$(LIB_EXT)
	$(QUIET)rm -rf ../bin

dist-clean :

install :
ifneq ("$(wildcard ../bin/$(NAME)$(EXE))","")
	$(QUIET)mkdir -p $(DESTDIR)$(EXE_INSTALL_DIR)
	$(QUIET)cp ./$(BUILD_DIR)/$(NAME) $(DESTDIR)$(EXE_INSTALL_DIR)/$(NAME)
  ifeq ($(user),root)
	$(QUIET)mkdir -p $(DESTDIR)$(DESKAPPS_DIR)
	$(QUIET)cp $(NAME).desktop $(DESTDIR)$(DESKAPPS_DIR)/$(NAME).desktop
	$(QUIET)cp $(NAME).svg $(DESTDIR)$(PIXMAPS_DIR)/$(NAME).svg
	$(QUIET)update-desktop-database || true
  endif
else
	@$(B_ECHO) "$(NAME)$(EXE) Standalone skipped$(reset)"
endif
ifneq ("$(wildcard ../bin/$(NAME).clap)","")
	@$(B_ECHO) "Install  $(NAME).clap to $(DESTDIR)$(CLAP_INSTAL_DIR)/$(reset)"
	$(QUIET)mkdir -p $(DESTDIR)$(CLAP_INSTAL_DIR)/
	$(QUIET)cp -r ../bin/$(NAME).clap $(DESTDIR)$(CLAP_INSTAL_DIR)/$(NAME).clap
	@$(B_ECHO) ". ., done$(reset)"
else
	@$(B_ECHO) "$(NAME).clap Clap skipped$(reset)"
endif
ifneq ("$(wildcard ../bin/$(NAME)vst.$(LIB_EXT))","")
	@$(B_ECHO) "Install  $(NAME)vst.$(LIB_EXT) to $(DESTDIR)$(VST2_INSTAL_DIR)/$(reset)"
	$(QUIET)mkdir -p $(DESTDIR)$(VST2_INSTAL_DIR)/
	$(QUIET)cp -r ../bin/$(NAME)vst.$(LIB_EXT) $(DESTDIR)$(VST2_INSTAL_DIR)/$(NAME)vst.$(LIB_EXT)
	@$(B_ECHO) ". ., done$(reset)"
else
	@$(B_ECHO) "$(NAME)vst.$(LIB_EXT) vst2 skipped$(reset)"
endif

uninstall :
	$(QUIET)rm -rf $(DESTDIR)$(EXE_INSTALL_DIR)/$(NAME)
	$(QUIET)rm -rf $(DESTDIR)$(DESKAPPS_DIR)/$(NAME).desktop
	$(QUIET)rm -rf $(DESTDIR)$(PIXMAPS_DIR)/$(NAME).svg
	$(QUIET)rm -rf $(DESTDIR)$(CLAP_INSTAL_DIR)/$(NAME).clap
	$(QUIET)rm -rf $(DESTDIR)$(VST2_INSTAL_DIR)/$(NAME)vst.$(LIB_EXT)

$(NAME).clap: $(CLAP_SOURCES) | info
	@$(B_ECHO) "Compiling $(NAME).clap $(reset)"
	$(QUIET)$(CXX) -MMD $(CXXFLAGS) -DPIC -shared $(CLAP_INCLUDE) $(CLAP_SOURCES) -o $(NAME).clap $(GUI_LDFLAGS) $(LDFLAGS)
ifeq (,$(filter yes,$(DEBUG)))
	$(QUIET)$(STRIP) -s -x -X -R .comment -R .note.ABI-tag $(NAME).clap
endif

$(NAME)vst.$(LIB_EXT): $(VST2_SOURCES) | info
	@$(B_ECHO) "Compiling $(NAME)vst.$(LIB_EXT) $(reset)"
	$(QUIET)$(CXX) -MMD $(CXXFLAGS) -DPIC -shared -Wno-multichar $(VST2_INCLUDE) $(VST2_SOURCES) -o $(NAME)vst.$(LIB_EXT) $(GUI_LDFLAGS) $(LDFLAGS)
ifeq (,$(filter yes,$(DEBUG)))
	$(QUIET)$(STRIP) -s -x -X -R .comment -R .note.ABI-tag $(NAME)vst.$(LIB_EXT)
endif

$(NAME) : $(OBJECTS) | info
ifeq ($(TARGET), Linux)
	@$(B_ECHO) "Build $@ $(reset)"
	$(QUIET)$(CXX) -MMD $(CXXFLAGS) $(OBJECTS) -o $(NAME)$(EXE) $(LDFLAGS) $(GUI_LDFLAGS)
ifneq ($(MAKECMDGOALS),debug)
	$(QUIET)$(STRIP) -s -x -X -R .note.ABI-tag $(NAME)$(EXE)
endif
	@$(B_ECHO) "=================== DONE =======================$(reset)"
endif
doc:
	#pass
