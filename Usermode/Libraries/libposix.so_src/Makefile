# Acess2 POSIX Emulation
# Makefile

-include ../Makefile.cfg

CPPFLAGS += 
CFLAGS   += -Wextra
ASFLAGS  +=
LDFLAGS  += -soname libposix.so -Map map.txt -lc

OBJ  = main.o unistd.o dirent.o stat.o utmpx.o termios.o
OBJ += pwd.o syslog.o sys_time.o sys_ioctl.o sys_resource.o
OBJ += fcntl.o clocks.o
DEPFILES := $(OBJ:%.o=%.d)
BIN = libposix.so

include ../Makefile.tpl

