#!/bin/bash
# tests/run_network_test.sh

echo "╔════════════════════════════════════════════════╗"
echo "║  RA-TDMAs+ Network Test Runner                 ║"
echo "╚════════════════════════════════════════════════╝"
echo ""

# Verifica se é root
if [ "$EUID" -ne 0 ]; then
    echo "❌ Run with sudo: sudo bash $0"
    exit 1
fi

# Verifica binário
if [ ! -f ./build/tdma_node ]; then
    echo "❌ Binary not found: ./build/tdma_node"
    echo "   Run 'make' first!"
    exit 1
fi

# Cleanup anterior
echo "🧹 Cleaning up previous run..."
pkill -9 -f tdma_node 2>/dev/null
rm -f /tmp/node*.log /tmp/node*.pid
sleep 1

# Setup network
echo "🔧 Setting up network namespaces..."
bash scripts/setup_dev_network.sh
sleep 2

# Start nodes
echo ""
echo "🚀 Starting nodes..."
for i in 1 2 3 4; do
    echo "   Starting node$i..."
    ip netns exec node$i ./build/tdma_node $i 4 2 > /tmp/node${i}.log 2>&1 &
    echo $! > /tmp/node${i}.pid
    sleep 0.5
done

echo ""
echo "✅ All nodes running!"
echo ""
echo "📊 Logs:"
echo "   tail -f /tmp/node1.log  (Node 1)"
echo "   tail -f /tmp/node2.log  (Node 2)"
echo "   tail -f /tmp/node3.log  (Node 3)"
echo "   tail -f /tmp/node4.log  (Node 4)"
echo ""
echo "🛑 To stop: sudo pkill -f tdma_node"
echo ""

# Aguarda 5 segundos e mostra status inicial
sleep 5

echo "═══════════════════════════════════════════════"
echo "  INITIAL STATUS (Node 1 - first 40 lines)"
echo "═══════════════════════════════════════════════"
head -n 40 /tmp/node1.log

echo ""
echo "▶️  Nodes running in background."
echo "   Monitor logs: tail -f /tmp/node*.log"
echo ""
echo "Press Enter to stop and show final stats..."
read

# Stop nodes
echo ""
echo "🛑 Stopping nodes..."
for i in 1 2 3 4; do
    if [ -f /tmp/node${i}.pid ]; then
        PID=$(cat /tmp/node${i}.pid)
        kill -INT $PID 2>/dev/null
        echo "   Stopped node$i (PID $PID)"
    fi
done

sleep 2

echo ""
echo "═══════════════════════════════════════════════"
echo "  FINAL STATS (last 50 lines of each node)"
echo "═══════════════════════════════════════════════"

for i in 1 2 3 4; do
    echo ""
    echo "--- NODE $i ---"
    tail -n 50 /tmp/node${i}.log | grep -E "(Heartbeats|Round:|Synchronized|adjustments)"
done

echo ""
echo "✅ Test complete!"
echo "   Full logs: /tmp/node*.log"
echo ""
