<<<<<<< HEAD
version https://git-lfs.github.com/spec/v1
oid sha256:4fbe3cb6d4092966da1e50e06be5d57956bf356ebc1e8b307069ba681761728e
size 1152
=======
YOLO = tiny_yolo_image

CXX       :=   g++
CC        :=   gcc 
YOLOOBJ   :=   objects/tiny_yolo_image.o

# linking libraries of OpenCV
LDFLAGS  =  $(shell pkg-config --libs opencv) -lpthread -lhineon -ln2cube -ldputils

CUR_DIR =   $(shell pwd)
MODEL   =   $(CUR_DIR)/model/dpu_tiny_yolo.elf
ARCH    =     $(shell uname -m | sed -e s/arm.*/armv71/ \
	-e s/aarch64.*/aarch64/ )

CFLAGS  :=  -O2 -Wall -Wpointer-arith -std=c++11 -ffast-math

ifeq ($(ARCH),armv71)
	CFLAGS +=  -mcpu=cortex-a9 -mfloat-abi=hard -mfpu=neon
endif
ifeq ($(ARCH),aarch64)
	CFLAGS += -mcpu=cortex-a53
endif

# Define source and object file directories
SRC_DIR = programs
OBJ_DIR = objects

# List of source files
SRC_FILES := $(wildcard $(SRC_DIR)/tiny_yolo_image.cpp)

# Generate a list of object files from source files
OBJ_FILES := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC_FILES))

.PHONY: all clean

all: $(YOLO)

# Rule to compile .cpp files into .o files
$(OBJ_DIR)/%.o : $(SRC_DIR)/%.cpp
	$(CXX) -c $(CFLAGS) $< -o $@

# Rule to build the final executable
$(YOLO): $(OBJ_FILES)
	$(CXX) $(CFLAGS) $^ $(MODEL) -o $@ $(LDFLAGS)

clean:
	rm -f $(OBJ_DIR)/*.o $(YOLO)
>>>>>>> origin/main
