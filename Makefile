# Makefile (versão completa atualizada)
CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -pthread -g
LDFLAGS = -pthread -lm

SRC_DIR = src
BUILD_DIR = build
TEST_DIR = tests

# -------------------------------------------
# Source files
# -------------------------------------------

# Topology
TOPO_SRCS = $(SRC_DIR)/topology/connectivity_matrix.c \
            $(SRC_DIR)/topology/spanning_tree.c

# Routing
ROUTING_SRCS = $(SRC_DIR)/routing/dijkstra.c \
               $(SRC_DIR)/routing/routing_manager.c

# Network (RA-TDMAs+ integration)
NETWORK_SRCS = $(SRC_DIR)/network/udp_transport.c \
               $(SRC_DIR)/network/tdma_node.c

# Sync (RA-TDMAs+)
SYNC_SRCS = $(SRC_DIR)/sync/ra_tdmas_sync.c

# Main program
MAIN_SRC = $(SRC_DIR)/main.c

# Test sources
TEST_SRCS = $(TEST_DIR)/test_topology.c
TEST_DIJKSTRA_SRCS = $(TEST_DIR)/test_dijkstra.c
TEST_COMPARISON_SRCS = $(TEST_DIR)/test_comparison.c
TEST_ROUTING_MANAGER_SRCS = $(TEST_DIR)/test_routing_manager.c

# -------------------------------------------
# Object files
# -------------------------------------------

TOPO_OBJS = $(TOPO_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
ROUTING_OBJS = $(ROUTING_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
NETWORK_OBJS = $(NETWORK_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
SYNC_OBJS = $(SYNC_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
MAIN_OBJ = $(MAIN_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

TEST_OBJS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/tests/%.o)
TEST_DIJKSTRA_OBJS = $(TEST_DIJKSTRA_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/tests/%.o)
TEST_COMPARISON_OBJS = $(TEST_COMPARISON_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/tests/%.o)
TEST_ROUTING_MANAGER_OBJS = $(TEST_ROUTING_MANAGER_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/tests/%.o)

# -------------------------------------------
# Targets
# -------------------------------------------

# Test binaries
TEST_BIN = $(BUILD_DIR)/test_topology
TEST_DIJKSTRA_BIN = $(BUILD_DIR)/test_dijkstra
TEST_COMPARISON_BIN = $(BUILD_DIR)/test_comparison
TEST_ROUTING_MANAGER_BIN = $(BUILD_DIR)/test_routing_manager

# Main daemon binary
TDMA_NODE_BIN = $(BUILD_DIR)/tdma_node

# Build all
all: $(TEST_BIN) $(TEST_DIJKSTRA_BIN) $(TEST_COMPARISON_BIN) \
     $(TEST_ROUTING_MANAGER_BIN) $(TDMA_NODE_BIN)

# -------------------------------------------
# Compilation rules
# -------------------------------------------

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/tests/%.o: $(TEST_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# -------------------------------------------
# Test executables
# -------------------------------------------

$(TEST_BIN): $(TOPO_OBJS) $(TEST_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS)

$(TEST_DIJKSTRA_BIN): $(TOPO_OBJS) $(ROUTING_OBJS) $(TEST_DIJKSTRA_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS)

$(TEST_COMPARISON_BIN): $(TOPO_OBJS) $(ROUTING_OBJS) $(TEST_COMPARISON_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS)

$(TEST_ROUTING_MANAGER_BIN): $(TOPO_OBJS) $(ROUTING_OBJS) $(TEST_ROUTING_MANAGER_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS)

# -------------------------------------------
# Main TDMA Node Daemon (RA-TDMAs+ enabled)
# -------------------------------------------

$(TDMA_NODE_BIN): $(TOPO_OBJS) $(ROUTING_OBJS) $(NETWORK_OBJS) \
                  $(SYNC_OBJS) $(MAIN_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS)
	@echo ""
	@echo "✅ Built: $(TDMA_NODE_BIN)"
	@echo ""

# -------------------------------------------
# Test targets
# -------------------------------------------

test: $(TEST_BIN) $(TEST_DIJKSTRA_BIN) $(TEST_COMPARISON_BIN) \
      $(TEST_ROUTING_MANAGER_BIN)
	@echo "=== Running topology tests ==="
	./$(TEST_BIN)
	@echo ""
	@echo "=== Running Dijkstra tests ==="
	./$(TEST_DIJKSTRA_BIN)
	@echo ""
	@echo "=== Running Comparison tests ==="
	./$(TEST_COMPARISON_BIN)
	@echo ""
	@echo "=== Running Routing Manager tests ==="
	./$(TEST_ROUTING_MANAGER_BIN)

# Individual test targets
test_topology: $(TEST_BIN)
	./$(TEST_BIN)

test_dijkstra: $(TEST_DIJKSTRA_BIN)
	./$(TEST_DIJKSTRA_BIN)

test_comparison: $(TEST_COMPARISON_BIN)
	./$(TEST_COMPARISON_BIN)

test_routing_manager: $(TEST_ROUTING_MANAGER_BIN)
	./$(TEST_ROUTING_MANAGER_BIN)

# -------------------------------------------
# Network integration test
# -------------------------------------------

network_test: $(TDMA_NODE_BIN)
	@echo ""
	@echo "╔════════════════════════════════════════════════╗"
	@echo "║  RA-TDMAs+ Network Integration Test            ║"
	@echo "╚════════════════════════════════════════════════╝"
	@echo ""
	@if [ ! -f tests/test_network_integration.sh ]; then \
		echo "❌ Test script not found!"; \
		exit 1; \
	fi
	@chmod +x tests/test_network_integration.sh
	@sudo bash tests/test_network_integration.sh

# -------------------------------------------
# Clean
# -------------------------------------------

clean:
	rm -rf $(BUILD_DIR)
	rm -f /tmp/node*.log /tmp/node*.pid
	@echo "✅ Clean complete"

.PHONY: all clean test test_topology test_dijkstra test_comparison \
        test_routing_manager network_test