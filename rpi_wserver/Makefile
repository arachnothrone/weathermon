# 1. Build protobuf: 	make adif
# 2. Build application: make all

# Compiler and flags
CC = g++
PROTOC = protoc
CFLAGS = -Wall -Wextra -std=c++17
LDFLAGS = -lprotobuf -lpthread
#LDFLAGS = -lprotobuf
DEBUG = -g

# OS specific flags
UNAME_S := $(shell uname -s)
UNAME_A := $(shell uname -a | grep -o "Ubuntu")
$(info UNAME_A = $(UNAME_A))

ifeq ($(UNAME_S),Darwin)
# macOS specific
# LDFLAGS = -framework OpenGL -lglfw
$(info MacOS: $(UNAME_S))
_OS_TYPE = 1
#elif ($(UNAME_S),Linux)
else ifeq ($(UNAME_A),Ubuntu)
# Ubuntu specific
$(info Ubuntu Linux: $(shell uname -a))
# LDFLAGS = -lGL -lglfw
_OS_TYPE = 3
else
# Raspbian specific
$(info Raspbian Linux: $(shell uname -a))
#LDFLAGS += -L/usr/lib/x86_64-linux-gnu/
_OS_TYPE = 2
endif
#else
## Raspbian specific
#$(info Raspbian Linux-2: $(shell uname -a))
## LDFLAGS = -lGL -lglfw
#_OS_TYPE = 2
#endif

#PROTOBUF_INCLUDE = /usr/local/include/google/protobuf
PROTOBUF_INCLUDE = /usr/include/google/protobuf

#PROTOBUF_BUILT_SOURCES = $(wildcard *.pb.cc)
#PROTOBUF_OBJS = $(PROTOBUF_BUILT_SOURCES:.pb.cc=.o)

# Source files and executable
#SRCS = $(wildcard *.cpp)
#SRCS += $(PROTOBUF_BUILT_SOURCES)

# arduino_data_if: arduino_data.proto
# 	$(PROTOC) -I=./ --cpp_out=./ arduino_data.proto

SRCS_CPP = $(wildcard *.cpp)
SRCS_PB_CC = $(wildcard *.pb.cc)
SRCS = $(SRCS_CPP) $(SRCS_PB_CC)
OBJS = $(SRCS:.cpp=.o)
$(info Sources: $(SRCS), $(SRCS_PB_CC))
$(info Objects: $(OBJS))

#OBJS = $(SRCS:.cpp=.o)

EXEC = rpiwserver
CFLAGS += -D OS_TYPE=$(_OS_TYPE) -I$(PROTOBUF_INCLUDE)
#CFLAGS += -D OS_TYPE=$(_OS_TYPE)

# Targets
all: adif $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(EXEC)
#	$(CC) $(CFLAGS) $(OBJS) -o $(EXEC)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.pb.cc
	$(CC) $(CFLAGS) -c -o $@ $<

# arduino_data_if
adif: arduino_data.proto
	$(PROTOC) -I=./ --cpp_out=./ arduino_data.proto

clean:
	rm -f $(OBJS) $(EXEC)

debug: CFLAGS += $(DEBUG)
debug: clean all


# bazel-bin/protoc
# bazel-bin/src/google/protobuf/compiler/cpp
