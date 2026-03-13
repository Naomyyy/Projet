# --- Directories ---
SRC_DIR  = src
INC_DIR  = include
OBJ_DIR  = obj
MAKE_DIR = make

include $(MAKE_DIR)/Make_linux.inc
#include $(MAKE_DIR)/Make_msys2.inc
#include $(MAKE_DIR)/Make_osx.inc
CXX = mpicxx

CXXFLAGS = -std=c++17 -I$(INC_DIR) -pg -fopenmp

ifdef DEBUG
CXXFLAGS += -g -O0 -Wall -fbounds-check -pedantic -D_GLIBCXX_DEBUG
CXXFLAGS2 = $(CXXFLAGS)
else
CXXFLAGS2 = $(CXXFLAGS) -O2 -march=native -Wall 
CXXFLAGS += -O3 -march=native -Wall

endif

# ---  Executable ---
EXEC = ant_simu.exe 


SRCS = $(wildcard $(SRC_DIR)/*.cpp)

OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

.PHONY: default all clean help

default: help

all: $(EXEC)


$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)


$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS2) -c $< -o $@


$(EXEC): $(OBJS)
	$(CXX) $(CXXFLAGS2) $^ -o $@ $(LIB)	

# --- Clean ---
clean:
	@rm -rf $(OBJ_DIR)/*.o $(EXEC) *~
	@echo "Cleaned up generated files."

help:
	@echo "Available targets : "
	@echo "    all            : compile all executables"
	@echo "    clean          : clean generated files"
	@echo "Add DEBUG=yes to compile in debug"
	@echo "Configuration :"
	@echo "    CXX      :    $(CXX)"
	@echo "    CXXFLAGS :    $(CXXFLAGS)"