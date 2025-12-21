#!/bin/bash
# tests/test_network_integration.sh

echo "╔════════════════════════════════════════════════╗"
echo "║  RA-TDMAs+ Network Integration Test            ║"
echo "╚════════════════════════════════════════════════╝"
echo ""

# Verifica se é root
if [ "$EUID" -ne 0 ]; then
    echo "❌ Please run as root: sudo $0"
    exit 1
fi

# Configuração
NUM_NODES=4
BUILD_DIR="./build"
NODE_BIN="$BUILD_DIR/tdma_node_main"

# Verifica se binário existe
if [ ! -f "$NODE_BIN" ]; then
    echo "❌ Binary not found: $NODE_BIN"
    echo "   Run 'make' first!"
    exit 1
fi

# Setup network namespaces
echo "🔧 Setting up network namespaces..."
bash scripts/setup_dev_network.sh

sleep 2

# Start nodes in background
echo ""
echo "🚀 Starting TDMA nodes..."
for i in $(seq 1 $NUM_NODES); do
    NODE="node$i"
    echo "   Starting $NODE..."
    
    ip netns exec $NODE $NODE_BIN $i $NUM_NODES 2 > /tmp/node${i}.log &
    NODE_PID=$!
    echo $NODE_PID > /tmp/node${i}.pid
    
    echo "   → PID: $NODE_PID"
done

echo ""
echo "✅ All nodes started!"
echo ""
echo "📊 Monitoring logs (Ctrl+C to stop)..."
echo "   Logs: /tmp/node*.log"
echo "   PIDs: /tmp/node*.pid"
echo ""

# Monitor logs
sleep 5

echo "═══════════════════════════════════════════════"
echo "  NODE 1 STATUS (first 30 lines)"
echo "═══════════════════════════════════════════════"
head -n 30 /tmp/node1.log

echo ""
echo "Press Enter to stop all nodes..."
read

# Stop all nodes
echo ""
echo "🛑 Stopping nodes..."
for i in $(seq 1 $NUM_NODES); do
    if [ -f /tmp/node${i}.pid ]; then
        PID=$(cat /tmp/node${i}.pid)
        echo "   Killing node$i (PID $PID)..."
        kill $PID 2>/dev/null
        rm /tmp/node${i}.pid
    fi
done

echo ""
echo "✅ Test complete!"
echo "   Check logs: tail -f /tmp/node*.log"