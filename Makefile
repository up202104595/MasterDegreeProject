# Makefile Completo (Topology + Routing + Network + Daemon)

# --- CONFIGURAÇÃO ---
CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -pthread -g
LDFLAGS = -pthread -lm

# Diretórios
SRC_DIR = src
BUILD_DIR = build
TEST_DIR = tests

# --- SOURCES ---

# 1. Topology
TOPO_SRCS = $(SRC_DIR)/topology/connectivity_matrix.c \
            $(SRC_DIR)/topology/spanning_tree.c

# 2. Routing
ROUTING_SRCS = $(SRC_DIR)/routing/dijkstra.c \
               $(SRC_DIR)/routing/routing_manager.c

# 3. Network (NOVO)
NETWORK_SRCS = $(SRC_DIR)/network/udp_transport.c \
               $(SRC_DIR)/network/tdma_node.c

# 4. Main (NOVO - O Executável do Nó)
MAIN_SRC = $(SRC_DIR)/main.c

# 5. Tests
TEST_TOPO_SRC = $(TEST_DIR)/test_topology.c
TEST_DIJK_SRC = $(TEST_DIR)/test_dijkstra.c
TEST_COMP_SRC = $(TEST_DIR)/test_comparison.c
TEST_RM_SRC   = $(TEST_DIR)/test_routing_manager.c

# --- OBJECTS ---
# Converte .c em .o dentro da pasta build/
TOPO_OBJS    = $(TOPO_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
ROUTING_OBJS = $(ROUTING_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
NETWORK_OBJS = $(NETWORK_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
MAIN_OBJ     = $(MAIN_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

TEST_TOPO_OBJ = $(TEST_TOPO_SRC:$(TEST_DIR)/%.c=$(BUILD_DIR)/tests/%.o)
TEST_DIJK_OBJ = $(TEST_DIJK_SRC:$(TEST_DIR)/%.c=$(BUILD_DIR)/tests/%.o)
TEST_COMP_OBJ = $(TEST_COMP_SRC:$(TEST_DIR)/%.c=$(BUILD_DIR)/tests/%.o)
TEST_RM_OBJ   = $(TEST_RM_SRC:$(TEST_DIR)/%.c=$(BUILD_DIR)/tests/%.o)

# --- BINARIES ---
TARGET_DAEMON = $(BUILD_DIR)/tdma_daemon

TEST_TOPO_BIN = $(BUILD_DIR)/test_topology
TEST_DIJK_BIN = $(BUILD_DIR)/test_dijkstra
TEST_COMP_BIN = $(BUILD_DIR)/test_comparison
TEST_RM_BIN   = $(BUILD_DIR)/test_routing_manager

# --- REGRAS GERAIS ---

all: $(TARGET_DAEMON) $(TEST_TOPO_BIN) $(TEST_DIJK_BIN) $(TEST_RM_BIN)

# Regra para compilar ficheiros da SRC (.c -> .o)
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Regra para compilar ficheiros de TESTS (.c -> .o)
$(BUILD_DIR)/tests/%.o: $(TEST_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# --- REGRAS DE LINKAGEM ---

# 1. Executável Principal (TDMA DAEMON)
$(TARGET_DAEMON): $(MAIN_OBJ) $(NETWORK_OBJS) $(ROUTING_OBJS) $(TOPO_OBJS)
	@echo "Linking Daemon..."
	$(CC) $^ -o $@ $(LDFLAGS)

# 2. Testes
$(TEST_TOPO_BIN): $(TEST_TOPO_OBJ) $(TOPO_OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

$(TEST_DIJK_BIN): $(TEST_DIJK_OBJ) $(TOPO_OBJS) $(ROUTING_OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

$(TEST_COMP_BIN): $(TEST_COMP_OBJ) $(TOPO_OBJS) $(ROUTING_OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

$(TEST_RM_BIN): $(TEST_RM_OBJ) $(ROUTING_OBJS) $(NETWORK_OBJS) $(TOPO_OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

# --- COMANDOS ÚTEIS ---

clean:
	rm -rf $(BUILD_DIR)

# Atalhos para correr testes
test_all: $(TEST_TOPO_BIN) $(TEST_DIJK_BIN) $(TEST_RM_BIN)
	./$(TEST_TOPO_BIN)
	./$(TEST_DIJK_BIN)
	./$(TEST_RM_BIN)

test_routing_manager: $(TEST_RM_BIN)
	./$(TEST_RM_BIN)

.PHONY: all clean test_all test_routing_manager