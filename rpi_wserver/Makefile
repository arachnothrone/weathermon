# Compiler and flags
CC = g++
CFLAGS = -Wall -Wextra -std=c++17
LDFLAGS = -lprotobuf -lpthread
#LDFLAGS = -lprotobuf
DEBUG = -g

# OS specific flags
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
	# macOS specific
	# LDFLAGS = -framework OpenGL -lglfw
	_OS_TYPE = 1
else
	# Raspbian specific
	# LDFLAGS = -lGL -lglfw
	_OS_TYPE = 2
endif

#PROTOBUF_INCLUDE = /usr/local/include/google/protobuf
PROTOBUF_INCLUDE = /usr/include/google/protobuf

# Source files and executable
SRCS = $(wildcard *.cpp)
#SRCP = $(wildcard *.pb.cc)
OBJS = $(SRCS:.cpp=.o)
#OBJP = $(SRCP:.pb.cc=.o)
EXEC = rpiwserver
CFLAGS += -D OS_TYPE=$(_OS_TYPE) -I$(PROTOBUF_INCLUDE)
#CFLAGS += -D OS_TYPE=$(_OS_TYPE)

# Targets
all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(EXEC)
#	$(CC) $(CFLAGS) $(OBJS) -o $(EXEC)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(EXEC)

debug: CFLAGS += $(DEBUG)
debug: clean all


# bazel-bin/protoc
# bazel-bin/src/google/protobuf/compiler/cpp
