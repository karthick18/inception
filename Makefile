CC := gcc
CFLAGS := -g -Wall -m32
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
