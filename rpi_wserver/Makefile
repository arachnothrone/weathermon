# Compiler and flags
CC = g++
CFLAGS = -Wall -Wextra -std=c++11

# OS specific flags
# UNAME_S := $(shell uname -s)
# ifeq ($(UNAME_S),Darwin)
# 	# macOS specific
# 	LDFLAGS = -framework OpenGL -lglfw
# else
# 	# Raspbian specific
# 	LDFLAGS = -lGL -lglfw
# endif

# Source files and executable
SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)
EXEC = rpiwserver

# Targets
all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(EXEC)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(EXEC)
