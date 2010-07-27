## Build with make ARCH_FLAGS=i686 to get the last inception thought of Fischer till
## the code buffer that morphs the thought is fixed for x86_64

CC := gcc
ARCH := $(shell uname -m | sed -e 's,i.86,i386,')
ifeq ($(ARCH),i386)
	ARCH_FLAGS := -m32 
else 
	ARCH_FLAGS := -m64
endif
CFLAGS := -g -Wall $(ARCH_FLAGS)
SRC_FILES := $(wildcard *.c)
OBJ_FILES := $(SRC_FILES:%.c=%.o)
LDLIBS := -lpthread -lrt
TARGET := inception

all: $(TARGET)

$(TARGET): $(OBJ_FILES)
	$(CC) $(CFLAGS) -g  -o $@ $^ $(LDLIBS)

%.o:%.c
	$(CC) $(CFLAGS) -c -o $@ $< 

clean:
	rm -f $(OBJ_FILES) *~ $(TARGET)
