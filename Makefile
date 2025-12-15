# Makefile
CC = gcc
CFLAGS = -Wall -Wextra -pthread -I./include -g -O0
LDFLAGS = -lm -lrt -lpthread

# Directories
SRC_DIR = src
BUILD_DIR = build
TEST_DIR = tests
INCLUDE_DIR = include

# Source files
TOPO_SRCS = $(SRC_DIR)/topology/connectivity_matrix.c \
            $(SRC_DIR)/topology/spanning_tree.c

TEST_SRCS = $(TEST_DIR)/test_topology.c

# Object files
TOPO_OBJS = $(TOPO_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
TEST_OBJS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/tests/%.o)

# Targets
TEST_BIN = $(BUILD_DIR)/test_topology

.PHONY: all clean test dirs

all: $(TEST_BIN)

# --- REGRAS DE COMPILAÇÃO CORRIGIDAS ---

# Compila código fonte normal (.c -> .o)
# O comando @mkdir -p $(dir $@) cria a pasta automaticamente antes de compilar
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Compila código de teste (.c -> .o)
$(BUILD_DIR)/tests/%.o: $(TEST_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Linkagem final
$(TEST_BIN): $(TOPO_OBJS) $(TEST_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS)

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -rf $(BUILD_DIR)

run: test