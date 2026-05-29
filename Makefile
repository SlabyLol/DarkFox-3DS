#---------------------------------------------------------------------------------
# DarkFox-3DS Makefile
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)

# devkitarm-rules 1.6.x änderte den Installationspfad –
# beide Varianten werden geprüft
RULES_MK := $(wildcard $(DEVKITPRO)/rules/3ds.mk)
ifeq ($(RULES_MK),)
  RULES_MK := $(wildcard $(DEVKITPRO)/devkitARM/3ds.mk)
endif
ifeq ($(RULES_MK),)
  RULES_MK := $(firstword $(wildcard $(DEVKITPRO)/*/3ds.mk))
endif
ifeq ($(RULES_MK),)
  $(error "3ds.mk nicht gefunden – bitte 3ds-dev installieren: dkp-pacman -S 3ds-dev")
endif

include $(RULES_MK)

#---------------------------------------------------------------------------------
TARGET      := DarkFox-3DS
BUILD       := build
SOURCES     := source
DATA        := data
INCLUDES    := include
GRAPHICS    := gfx
ROMFS       :=

APP_TITLE       := DarkFox-3DS
APP_DESCRIPTION := A multi-purpose utility for Nintendo 3DS
APP_AUTHOR      := DarkFox Team

ICON := resources/icon.png

#---------------------------------------------------------------------------------
ARCH    := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS  := -g -Wall -O2 -mword-relocations \
            -ffunction-sections \
            $(ARCH)

CFLAGS  += $(INCLUDE) -DARM11 -D_3DS

CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17

ASFLAGS := -g $(ARCH)
LDFLAGS  = -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS := -lctru -lm

LIBDIRS := $(CTRULIB)

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT  := $(CURDIR)/$(TARGET)
export TOPDIR  := $(CURDIR)

export VPATH   := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                  $(foreach dir,$(DATA),$(CURDIR)/$(dir)) \
                  $(foreach dir,$(GRAPHICS),$(CURDIR)/$(dir))

export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES    := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
PICAFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.v.pica)))
SHLISTFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.shlist)))
GFXFILES    := $(foreach dir,$(GRAPHICS),$(notdir $(wildcard $(dir)/*.t3s)))
BINFILES    := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifeq ($(strip $(CPPFILES)),)
    export LD := $(CC)
else
    export LD := $(CXX)
endif

export OFILES_SOURCES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES_BIN     := $(addsuffix .o,$(BINFILES))
export OFILES         := $(PICAFILES:.v.pica=.shbin.o) \
                         $(SHLISTFILES:.shlist=.shbin.o) \
                         $(if $(strip $(GFXFILES)),$(GFXFILES:.t3s=.t3x.o),) \
                         $(OFILES_BIN) \
                         $(OFILES_SOURCES)

export HFILES   := $(PICAFILES:.v.pica=_shbin.h) $(SHLISTFILES:.shlist=_shbin.h) \
                   $(addsuffix .h,$(subst .,_,$(BINFILES))) \
                   $(GFXFILES:.t3s=.t3x.h)

export INCLUDE  := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                   $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                   -I$(CURDIR)/$(BUILD)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export _3DSXDEPS := $(if $(NO_SMDH),,$(OUTPUT).smdh)

ifeq ($(strip $(ICON)),)
    icons := $(wildcard *.png)
    ifneq (,$(findstring $(TARGET).png,$(icons)))
        export APP_ICON := $(TOPDIR)/$(TARGET).png
    else
        ifneq (,$(findstring icon.png,$(icons)))
            export APP_ICON := $(TOPDIR)/icon.png
        endif
    endif
else
    export APP_ICON := $(TOPDIR)/$(ICON)
endif

ifeq ($(strip $(NO_SMDH)),)
    export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh
endif

ifneq ($(ROMFS),)
    export _3DSXFLAGS += --romfs=$(CURDIR)/$(ROMFS)
endif

.PHONY: $(BUILD) clean all

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(OUTPUT).smdh $(TARGET).elf

#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------

$(OUTPUT).3dsx: $(OUTPUT).elf $(_3DSXDEPS)

$(OFILES_SOURCES): $(HFILES)

%.bin.o %_bin.h : %.bin
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPSDIR)/*.d

endif
