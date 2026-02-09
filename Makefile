# Makefile - Complete TDMA System
# Enhanced version with comprehensive build and test targets

# ============================================
# Compiler and Flags
# ============================================
CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -pthread -g -O2
LDFLAGS = -pthread -lm
DEBUG_FLAGS = -DDEBUG -g3 -O0
STRICT_FLAGS = -Werror

# ============================================
# Directory Structure
# ============================================
SRC_DIR = src
BUILD_DIR = build
TEST_DIR = tests
SCRIPTS_DIR = scripts
INCLUDE_DIR = include
LOG_DIR = logs

# ============================================
# Source Files Organization
# ============================================
TOPO_SRCS = $(SRC_DIR)/topology/connectivity_matrix.c \
            $(SRC_DIR)/topology/spanning_tree.c

ROUTING_SRCS = $(SRC_DIR)/routing/dijkstra.c \
               $(SRC_DIR)/routing/routing_manager.c

NETWORK_SRCS = $(SRC_DIR)/network/udp_transport.c \
               $(SRC_DIR)/network/tdma_node.c \
               $(SRC_DIR)/network/ip_routing_manager.c \
               $(SRC_DIR)/network/data_streaming.c

SYNC_SRCS = $(SRC_DIR)/sync/ra_tdmas_sync.c

DATA_SRCS = $(SRC_DIR)/data/tx_queue.c

MAIN_SRC = $(SRC_DIR)/main.c

# All source files
ALL_SRCS = $(TOPO_SRCS) $(ROUTING_SRCS) $(NETWORK_SRCS) $(SYNC_SRCS) $(DATA_SRCS) $(MAIN_SRC)

# ============================================
# Object Files
# ============================================
TOPO_OBJS = $(TOPO_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
ROUTING_OBJS = $(ROUTING_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
NETWORK_OBJS = $(NETWORK_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
SYNC_OBJS = $(SYNC_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
DATA_OBJS = $(DATA_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
MAIN_OBJ = $(MAIN_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

ALL_OBJS = $(TOPO_OBJS) $(ROUTING_OBJS) $(NETWORK_OBJS) $(SYNC_OBJS) $(DATA_OBJS) $(MAIN_OBJ)

# ============================================
# Test Source Files (if exist)
# ============================================
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TEST_BINS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/test_%)

# ============================================
# Main Targets
# ============================================
TDMA_NODE = $(BUILD_DIR)/tdma_node

# ============================================
# Build Rules
# ============================================
.PHONY: all
all: directories $(TDMA_NODE)
	@echo ""
	@echo "╔════════════════════════════════════════════════╗"
	@echo "║  ✅ Build Complete                             ║"
	@echo "╚════════════════════════════════════════════════╝"
	@echo ""
	@echo "Binary: $(TDMA_NODE)"
	@echo ""

# Create necessary directories
.PHONY: directories
directories:
	@mkdir -p $(BUILD_DIR)/topology
	@mkdir -p $(BUILD_DIR)/routing
	@mkdir -p $(BUILD_DIR)/network
	@mkdir -p $(BUILD_DIR)/sync
	@mkdir -p $(BUILD_DIR)/data
	@mkdir -p $(LOG_DIR)

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# Link main executable
$(TDMA_NODE): $(ALL_OBJS)
	@mkdir -p $(dir $@)
	@echo "Linking $(TDMA_NODE)..."
	@$(CC) $^ -o $@ $(LDFLAGS)
	@echo "✅ Built: $(TDMA_NODE)"

# Debug build
.PHONY: debug
debug: CFLAGS += $(DEBUG_FLAGS)
debug: clean all
	@echo "🐛 Debug build complete"

# Strict build (warnings as errors)
.PHONY: strict
strict: CFLAGS += $(STRICT_FLAGS)
strict: clean all
	@echo "⚠️  Strict build complete (all warnings are errors)"

# ============================================
# Unit Tests
# ============================================
.PHONY: tests
tests: $(TEST_BINS)
	@echo ""
	@echo "╔════════════════════════════════════════════════╗"
	@echo "║  Running Unit Tests                            ║"
	@echo "╚════════════════════════════════════════════════╝"
	@echo ""
	@for test in $(TEST_BINS); do \
		echo "Running $$test..."; \
		$$test || exit 1; \
	done
	@echo ""
	@echo "✅ All unit tests passed"

$(BUILD_DIR)/test_%: $(TEST_DIR)/test_%.c $(filter-out $(MAIN_OBJ), $(ALL_OBJS))
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# ============================================
# Network Setup
# ============================================
.PHONY: setup_network
setup_network: $(TDMA_NODE)
	@echo ""
	@echo "╔════════════════════════════════════════════════╗"
	@echo "║  Setting Up Virtual Network                    ║"
	@echo "╚════════════════════════════════════════════════╝"
	@echo ""
	@if [ ! -f $(SCRIPTS_DIR)/setup_network.sh ]; then \
		echo "❌ Error: setup_network.sh not found!"; \
		exit 1; \
	fi
	@chmod +x $(SCRIPTS_DIR)/setup_network.sh
	@sudo $(SCRIPTS_DIR)/setup_network.sh

.PHONY: teardown_network
teardown_network:
	@echo ""
	@echo "╔════════════════════════════════════════════════╗"
	@echo "║  Tearing Down Virtual Network                  ║"
	@echo "╚════════════════════════════════════════════════╝"
	@echo ""
	@if [ -f $(SCRIPTS_DIR)/teardown_network.sh ]; then \
		chmod +x $(SCRIPTS_DIR)/teardown_network.sh; \
		sudo $(SCRIPTS_DIR)/teardown_network.sh; \
	else \
		echo "Killing TDMA processes..."; \
		sudo pkill -9 tdma_node || true; \
		echo "Removing network namespaces..."; \
		for ns in node1 node2 node3 node4; do \
			sudo ip netns del $$ns 2>/dev/null || true; \
		done; \
	fi
	@echo "✅ Network teardown complete"

# ============================================
# System Tests
# ============================================
.PHONY: test_full
test_full: $(TDMA_NODE)
	@echo ""
	@echo "╔════════════════════════════════════════════════╗"
	@echo "║  Complete System Test                          ║"
	@echo "║  (TDMA + Routing + Streaming + Recovery)       ║"
	@echo "╚════════════════════════════════════════════════╝"
	@echo ""
	@if [ -f $(SCRIPTS_DIR)/test_full_system.sh ]; then \
		chmod +x $(SCRIPTS_DIR)/test_full_system.sh; \
		sudo $(SCRIPTS_DIR)/test_full_system.sh; \
	elif [ -f ./test_full_system.sh ]; then \
		chmod +x ./test_full_system.sh; \
		sudo ./test_full_system.sh; \
	else \
		echo "❌ Error: test_full_system.sh not found!"; \
		echo "   Looked in: $(SCRIPTS_DIR)/ and ./"; \
		exit 1; \
	fi

.PHONY: test_multihop
test_multihop: $(TDMA_NODE)
	@echo ""
	@echo "╔════════════════════════════════════════════════╗"
	@echo "║  Multi-Hop Forwarding Test                     ║"
	@echo "╚════════════════════════════════════════════════╝"
	@echo ""
	@if [ -f $(SCRIPTS_DIR)/test_multihop_forwarding.sh ]; then \
		chmod +x $(SCRIPTS_DIR)/test_multihop_forwarding.sh; \
		sudo $(SCRIPTS_DIR)/test_multihop_forwarding.sh; \
	elif [ -f ./test_multihop_forwarding.sh ]; then \
		chmod +x ./test_multihop_forwarding.sh; \
		sudo ./test_multihop_forwarding.sh; \
	else \
		echo "❌ Error: test_multihop_forwarding.sh not found!"; \
		echo "   Looked in: $(SCRIPTS_DIR)/ and ./"; \
		exit 1; \
	fi

.PHONY: test_diamond
test_diamond: $(TDMA_NODE)
	@echo ""
	@echo "╔════════════════════════════════════════════════╗"
	@echo "║  Diamond Topology + Failure Test               ║"
	@echo "╚════════════════════════════════════════════════╝"
	@echo ""
	@if [ -f $(SCRIPTS_DIR)/test_diamond_rerouting.sh ]; then \
		chmod +x $(SCRIPTS_DIR)/test_diamond_rerouting.sh; \
		sudo $(SCRIPTS_DIR)/test_diamond_rerouting.sh; \
	else \
		echo "❌ Error: test_diamond_rerouting.sh not found!"; \
		exit 1; \
	fi

.PHONY: test_sync
test_sync: $(TDMA_NODE)
	@echo ""
	@echo "╔════════════════════════════════════════════════╗"
	@echo "║  TDMA Synchronization Test                     ║"
	@echo "╚════════════════════════════════════════════════╝"
	@echo ""
	@if [ -f $(SCRIPTS_DIR)/test_sync.sh ]; then \
		chmod +x $(SCRIPTS_DIR)/test_sync.sh; \
		sudo $(SCRIPTS_DIR)/test_sync.sh; \
	else \
		echo "⚠️  Sync test script not found"; \
	fi

.PHONY: test_streaming
test_streaming: $(TDMA_NODE)
	@echo ""
	@echo "╔════════════════════════════════════════════════╗"
	@echo "║  Data Streaming Test                           ║"
	@echo "╚════════════════════════════════════════════════╝"
	@echo ""
	@if [ -f $(SCRIPTS_DIR)/test_streaming.sh ]; then \
		chmod +x $(SCRIPTS_DIR)/test_streaming.sh; \
		sudo $(SCRIPTS_DIR)/test_streaming.sh; \
	else \
		echo "⚠️  Streaming test script not found"; \
	fi

# Run all tests
.PHONY: test_all
test_all: test_sync test_multihop test_streaming test_diamond test_full
	@echo ""
	@echo "╔════════════════════════════════════════════════╗"
	@echo "║  ✅ All System Tests Complete                  ║"
	@echo "╚════════════════════════════════════════════════╝"
	@echo ""

# ============================================
# Manual Network Operations
# ============================================
.PHONY: run_network
run_network: $(TDMA_NODE)
	@echo ""
	@echo "╔════════════════════════════════════════════════╗"
	@echo "║  Starting TDMA Network                         ║"
	@echo "╚════════════════════════════════════════════════╝"
	@echo ""
	@if [ -f $(SCRIPTS_DIR)/run_virtual_network.sh ]; then \
		chmod +x $(SCRIPTS_DIR)/run_virtual_network.sh; \
		sudo $(SCRIPTS_DIR)/run_virtual_network.sh; \
	else \
		echo "❌ Error: run_virtual_network.sh not found!"; \
		exit 1; \
	fi

.PHONY: stop_network
stop_network:
	@echo "Stopping TDMA network..."
	@sudo pkill -INT tdma_node || true
	@sleep 2
	@sudo pkill -9 tdma_node || true
	@echo "✅ Network stopped"

# ============================================
# Monitoring and Debugging
# ============================================
.PHONY: logs
logs:
	@echo ""
	@echo "╔════════════════════════════════════════════════╗"
	@echo "║  Recent Log Entries                            ║"
	@echo "╚════════════════════════════════════════════════╝"
	@echo ""
	@tail -n 50 $(LOG_DIR)/*.log 2>/dev/null || echo "No logs found"

.PHONY: watch_logs
watch_logs:
	@echo "Watching logs (Ctrl+C to stop)..."
	@tail -f $(LOG_DIR)/*.log

.PHONY: status
status:
	@echo ""
	@echo "╔════════════════════════════════════════════════╗"
	@echo "║  TDMA System Status                            ║"
	@echo "╚════════════════════════════════════════════════╝"
	@echo ""
	@echo "Running Processes:"
	@ps aux | grep tdma_node | grep -v grep || echo "No TDMA processes running"
	@echo ""
	@echo "Network Namespaces:"
	@sudo ip netns list 2>/dev/null || echo "No namespaces found"
	@echo ""
	@echo "Log Files:"
	@ls -lh $(LOG_DIR)/*.log 2>/dev/null || echo "No log files found"

# ============================================
# Code Quality
# ============================================
.PHONY: check
check:
	@echo "Running static analysis..."
	@which cppcheck > /dev/null 2>&1 && \
		cppcheck --enable=all --inconclusive --std=c11 $(SRC_DIR) || \
		echo "⚠️  cppcheck not installed"

.PHONY: format
format:
	@echo "Formatting code..."
	@which clang-format > /dev/null 2>&1 && \
		find $(SRC_DIR) $(INCLUDE_DIR) -name '*.c' -o -name '*.h' | xargs clang-format -i || \
		echo "⚠️  clang-format not installed"

.PHONY: lint
lint:
	@echo "Running linter..."
	@which splint > /dev/null 2>&1 && \
		splint $(SRC_DIR)/**/*.c || \
		echo "⚠️  splint not installed"

# ============================================
# Cleanup
# ============================================
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@echo "Cleaning temporary files..."
	@rm -f /tmp/node*.log /tmp/node*.pid
	@rm -f /tmp/tdma_*.log
	@echo "Cleaning log directory..."
	@rm -rf $(LOG_DIR)/*.log
	@echo "✅ Clean complete"

.PHONY: clean_all
clean_all: clean stop_network teardown_network
	@echo "✅ Full cleanup complete"

# ============================================
# Installation
# ============================================
.PHONY: install
install: $(TDMA_NODE)
	@echo "Installing TDMA node..."
	@sudo install -m 755 $(TDMA_NODE) /usr/local/bin/
	@echo "✅ Installed to /usr/local/bin/tdma_node"

.PHONY: uninstall
uninstall:
	@echo "Uninstalling TDMA node..."
	@sudo rm -f /usr/local/bin/tdma_node
	@echo "✅ Uninstalled"

# ============================================
# Dependencies
# ============================================
.PHONY: deps
deps:
	@echo "Checking dependencies..."
	@which gcc > /dev/null || echo "❌ gcc not found"
	@which make > /dev/null || echo "❌ make not found"
	@echo "✅ Basic dependencies OK"

# ============================================
# Documentation
# ============================================
.PHONY: help
help:
	@echo ""
	@echo "╔════════════════════════════════════════════════╗"
	@echo "║  TDMA System - Available Commands              ║"
	@echo "╚════════════════════════════════════════════════╝"
	@echo ""
	@echo "🔨 Build Commands:"
	@echo "  make              - Build TDMA node"
	@echo "  make debug        - Build with debug symbols"
	@echo "  make clean        - Clean build artifacts"
	@echo "  make clean_all    - Clean everything (build + network)"
	@echo ""
	@echo "🌐 Network Setup:"
	@echo "  make setup_network    - Setup virtual network (required first)"
	@echo "  make teardown_network - Cleanup network namespaces"
	@echo ""
	@echo "🧪 Testing:"
	@echo "  make test_full      - Complete system test (RECOMMENDED)"
	@echo "  make test_multihop  - Test IP forwarding"
	@echo "  make test_diamond   - Diamond topology + recovery"
	@echo "  make test_sync      - TDMA synchronization test"
	@echo "  make test_streaming - Data streaming test"
	@echo "  make test_all       - Run all system tests"
	@echo "  make tests          - Run unit tests"
	@echo ""
	@echo "🚀 Manual Operations:"
	@echo "  make run_network  - Run 4-node network manually"
	@echo "  make stop_network - Stop running network"
	@echo ""
	@echo "📊 Monitoring:"
	@echo "  make status      - Show system status"
	@echo "  make logs        - Show recent logs"
	@echo "  make watch_logs  - Watch logs in real-time"
	@echo ""
	@echo "🔧 Code Quality:"
	@echo "  make check  - Run static analysis"
	@echo "  make format - Format code"
	@echo "  make lint   - Run linter"
	@echo ""
	@echo "📦 Installation:"
	@echo "  make install   - Install to /usr/local/bin"
	@echo "  make uninstall - Remove installation"
	@echo ""
	@echo "📖 Quick Start:"
	@echo "  1. make clean && make"
	@echo "  2. make setup_network"
	@echo "  3. make test_full"
	@echo ""
	@echo "🆘 Troubleshooting:"
	@echo "  make clean_all - Full cleanup and reset"
	@echo "  make status    - Check current state"
	@echo "  make logs      - View error logs"
	@echo ""

.DEFAULT_GOAL := help

# ============================================
# Dependency Tracking
# ============================================
-include $(ALL_OBJS:.o=.d)

$(BUILD_DIR)/%.d: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -MM -MT $(@:.d=.o) $< > $@
