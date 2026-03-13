# --- Directories ---
SRC_DIR  = src
INC_DIR  = include
OBJ_DIR  = obj
MAKE_DIR = make

include $(MAKE_DIR)/Make_linux.inc

# --- Compilador MPI ---
# mpic++ é um wrapper que chama o g++ adicionando as bibliotecas MPI
CXX = mpic++

# --- Flags ---
# Adicionamos -fopenmp para garantir que as diretivas #pragma omp funcionem
CXXFLAGS = -std=c++17 -I$(INC_DIR) -pg -fopenmp

ifdef DEBUG
    CXXFLAGS += -g -O0 -Wall -fbounds-check -pedantic -D_GLIBCXX_DEBUG
    CXXFLAGS2 = $(CXXFLAGS)
else
    # O3 e march=native são essenciais para a vectorização (SIMD)
    CXXFLAGS2 = $(CXXFLAGS) -O2 -march=native -Wall 
    CXXFLAGS += -O3 -march=native -Wall
endif

# ---  Executable ---
EXEC = ant_simu_mpi.exe 

# --- Sources & Objects ---
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

# --- Libraries ---
# Certifique-se que $(LIB) contém as flags da SDL2 (ex: -lSDL2)
# Adicionamos -lpthread que o MPI costuma exigir
LIBS = $(LIB) -lpthread -lSDL2

.PHONY: default all clean help

default: help

all: $(EXEC)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Compilação dos objetos
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Linkagem final
$(EXEC): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS) 

# --- Clean ---
clean:
	@rm -rf $(OBJ_DIR)/*.o $(EXEC) $(OBJ_DIR) gmon.out
	@echo "Cleaned up generated files."

help:
	@echo "Available targets : "
	@echo "    all            : compile the MPI + OpenMP executable"
	@echo "    clean          : clean generated files"
	@echo "    Usage          : mpirun -np <num_procs> ./$(EXEC)"
	@echo "Add DEBUG=yes to compile in debug"
	@echo "Configuration :"
	@echo "    CXX      :    $(CXX)"
	@echo "    CXXFLAGS :    $(CXXFLAGS)"