# Acess Dynamic Linker (ELF) Version 1
#  LD-ACESS.SO
#  Makefile

-include ../Makefile.cfg

OBJ := main.o lib.o loadlib.o export.o elf.o pe.o
OBJ += arch/$(ARCHDIR).ao_
BIN = ld-acess.so
EXTRABIN := libld-acess.so
EXTRACLEAN = $(_OBJPREFIX)_stublib.o
INCFILES := sys/sys.h

CFLAGS   = -g -Wall -fno-builtin -fno-stack-protector -fPIC
# -fno-leading-underscore
CFLAGS  += $(CPPFLAGS) -Werror
LDFLAGS  = -g -T arch/$(ARCHDIR).ld -Map map.txt --export-dynamic

ifeq ($(ARCH),native)
XBIN := $(addprefix $(OUTPUTDIR)Libs/,$(EXTRABIN)) 
$(XBIN): obj-$(ARCH)/_stublib.o
all: $(XBIN)
LDFLAGS := 
BIN := 
OBJ :=
endif

include ../Makefile.tpl

# create libld-acess.so
$(_XBIN): $(_OBJPREFIX)_stublib.o
	@echo [LD] -o -shared libld-acess.so
	$(LD) -shared -o $@ $< $(LDFLAGS)
#	@$(LD) $(LDFLAGS) -o $@ $(OBJ)


# Override .ao to look in the object prefix for the source
$(_OBJPREFIX)arch/$(ARCHDIR).ao_: $(_OBJPREFIX)arch/$(ARCHDIR).$(ASSUFFIX)
	@echo [AS] -o $@
	@mkdir -p $(dir $@)
	@$(AS) $(ASFLAGS) -o $@ $<

#.PRECIOUS: $(OBJ:%.ao=%.asm)

# Preprocessing objects if needed
$(_OBJPREFIX)arch/$(ARCHDIR).$(ASSUFFIX): arch/$(ARCHDIR).$(ASSUFFIX).h arch/syscalls.s.h
	@echo [CPP] -o $@
	@mkdir -p $(dir $@)
	@$(CPP) $(CPPFLAGS) -P -D__ASSEMBLER__ $< -o $@

$(_OBJPREFIX)_stublib.o: arch/syscalls.s.h

