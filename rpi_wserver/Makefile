# Compiler and flags
CC = g++
CFLAGS = -Wall -Wextra -std=c++14
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

# Source files and executable
SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)
EXEC = rpiwserver
CFLAGS += -D OS_TYPE=$(_OS_TYPE)

# Targets
all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(EXEC)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(EXEC)

debug: CFLAGS += $(DEBUG)
debug: clean all
