## Local builds with make ARCH_FLAGS=i686 to get the last inception thought of Fischer till
## the code buffer that morphs the thought is fixed for x86_64
## To cross-compile for eg: MIPS (32 bit)
## build with make CROSS_COMPILE=<everything-without-gcc-suffix> ARCH_FLAGS= 

CROSS_COMPILE :=
CC := $(CROSS_COMPILE)gcc
UNAME := $(shell uname)
ARCH := $(shell uname -m | sed -e 's,i.86,i386,')
ifeq ($(ARCH),i386)
	ARCH_FLAGS := -m32 
else 
	ifeq ($(ARCH), x86_64)
		ARCH_FLAGS := -m64
	else
		ARCH_FLAGS :=
	endif
endif
CFLAGS := -g -Wall $(ARCH_FLAGS)
SRC_FILES := $(wildcard *.c)
OBJ_FILES := $(SRC_FILES:%.c=%.o)
LDLIBS := -lpthread
ifeq ("$(UNAME)", "Linux")
	LDLIBS += -lrt
endif
TARGET := inception

all: $(TARGET)

$(TARGET): $(OBJ_FILES)
	$(CC) $(CFLAGS) -g  -o $@ $^ $(LDLIBS)

%.o:%.c
	$(CC) $(CFLAGS) -c -o $@ $< 

clean:
	rm -f $(OBJ_FILES) *~ $(TARGET)

install:
	cp $(TARGET) /usr/local/bin

uninstall:
	rm /usr/local/bin/$(TARGET)
