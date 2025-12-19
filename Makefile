# Makefile (versão completa com test_comparison)
# --- INÍCIO DO MAKEFILE ---
# Compilador e Flags
CC = gcc
# Inclui a pasta 'include' e ativa threads (pthread)
CFLAGS = -Wall -Wextra -Iinclude -pthread -g
LDFLAGS = -pthread -lm

# Definição dos diretórios
SRC_DIR = src
BUILD_DIR = build
TEST_DIR = tests

# -------------------------------------------

# Source files
TOPO_SRCS = $(SRC_DIR)/topology/connectivity_matrix.c \
            $(SRC_DIR)/topology/spanning_tree.c

ROUTING_SRCS = $(SRC_DIR)/routing/dijkstra.c

TEST_SRCS = $(TEST_DIR)/test_topology.c
TEST_DIJKSTRA_SRCS = $(TEST_DIR)/test_dijkstra.c
TEST_COMPARISON_SRCS = $(TEST_DIR)/test_comparison.c  # <--- NOVO

# Object files
TOPO_OBJS = $(TOPO_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
ROUTING_OBJS = $(ROUTING_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
TEST_OBJS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/tests/%.o)
TEST_DIJKSTRA_OBJS = $(TEST_DIJKSTRA_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/tests/%.o)
TEST_COMPARISON_OBJS = $(TEST_COMPARISON_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/tests/%.o)  # <--- NOVO

# Targets
TEST_BIN = $(BUILD_DIR)/test_topology
TEST_DIJKSTRA_BIN = $(BUILD_DIR)/test_dijkstra
TEST_COMPARISON_BIN = $(BUILD_DIR)/test_comparison  # <--- NOVO

all: $(TEST_BIN) $(TEST_DIJKSTRA_BIN) $(TEST_COMPARISON_BIN)  # <--- ATUALIZADO

# Regras de compilação
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/tests/%.o: $(TEST_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Targets dos executáveis
$(TEST_BIN): $(TOPO_OBJS) $(TEST_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS)

$(TEST_DIJKSTRA_BIN): $(TOPO_OBJS) $(ROUTING_OBJS) $(TEST_DIJKSTRA_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS)

# <--- NOVO TARGET ---
$(TEST_COMPARISON_BIN): $(TOPO_OBJS) $(ROUTING_OBJS) $(TEST_COMPARISON_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS)

# Target para rodar todos os testes
test: $(TEST_BIN) $(TEST_DIJKSTRA_BIN) $(TEST_COMPARISON_BIN)  # <--- ATUALIZADO
	@echo "=== Running topology tests ==="
	./$(TEST_BIN)
	@echo ""
	@echo "=== Running Dijkstra tests ==="
	./$(TEST_DIJKSTRA_BIN)
	@echo ""
	@echo "=== Running Comparison tests ===" # <--- NOVO
	./$(TEST_COMPARISON_BIN)

# Targets individuais (útil para debugging)
test_topology: $(TEST_BIN)
	./$(TEST_BIN)

test_dijkstra: $(TEST_DIJKSTRA_BIN)
	./$(TEST_DIJKSTRA_BIN)

test_comparison: $(TEST_COMPARISON_BIN)  # <--- NOVO
	./$(TEST_COMPARISON_BIN)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean test test_topology test_dijkstra test_comparison  # <--- ATUALIZADO