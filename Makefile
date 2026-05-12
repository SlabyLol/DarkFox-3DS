ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/3ds_rules

TARGET		:=	DarkFox-3DS
BUILD		:=	build
SOURCES		:=	source
INCLUDES	:=	include

# Pfad zur libctru
CTRU_LIB_PATH := /opt/devkitpro/libctru/lib

CFLAGS	:=	-g -Wall -O2 -mword-relocations \
			-fomit-frame-pointer -ffunction-sections \
			$(ARCH)

CFLAGS	+=	$(INCLUDE) -D__3DS__

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11

ASFLAGS	:=	-g $(ARCH)

# 3dsx_crt0.o per find im gesamten devkitpro-Baum suchen (zuverlässiger als gcc -print-file-name)
CRT0_PATH	:=	$(shell find /opt/devkitpro -name "3dsx_crt0.o" 2>/dev/null | head -1)

ifeq ($(CRT0_PATH),)
  $(error "FEHLER: 3dsx_crt0.o nicht gefunden! Bitte 3ds-dev installieren: dkp-pacman -S 3ds-dev")
endif

CRT0_DIR	:=	$(dir $(CRT0_PATH))
$(info 3dsx_crt0.o gefunden: $(CRT0_PATH))

LDFLAGS	:=	-specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LDFLAGS	+=	-B$(CRT0_DIR)
LDFLAGS	+=	-L$(CTRU_LIB_PATH) -L$(DEVKITPRO)/libctru/lib

LIBS	:= -lctru -lm

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

export OFILES	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
					-I$(DEVKITPRO)/libctru/include \
					-I$(CURDIR)/$(BUILD)

.PHONY: $(BUILD) clean all

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).elf $(TARGET).smdh

else

DEPENDS	:=	$(OFILES:.o=.d)

$(OUTPUT).3dsx	:	$(OUTPUT).elf

$(OUTPUT).elf	:	$(OFILES)
	@echo linking $(notdir $@)
	$(CXX) $(LDFLAGS) $(OFILES) $(LIBS) -o $@

%.o: %.cpp
	@echo $(notdir $<)
	$(CXX) -MMD -MP -MF $(DEPSDIR)/$*.d $(CXXFLAGS) -c $< -o $@

%.o: %.c
	@echo $(notdir $<)
	$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(CFLAGS) -c $< -o $@

%.o: %.s
	@echo $(notdir $<)
	$(AS) -g $(ASFLAGS) -c $< -o $@

-include $(DEPENDS)

endif
