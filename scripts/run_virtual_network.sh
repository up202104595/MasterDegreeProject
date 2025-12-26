#!/bin/bash
# scripts/run_virtual_network.sh
echo "╔════════════════════════════════════════════════╗"
echo "║  TDMA Virtual Network Runner                   ║"
echo "╚════════════════════════════════════════════════╝"

if [ "$EUID" -ne 0 ]; then
    echo "❌ Error: Please run as root (sudo)"
    exit 1
fi

# Check if network is setup
if ! ip netns list | grep -q "node1"; then
    echo ""
    echo "🌐 Setting up network namespaces..."
    ./scripts/setup_dev_network.sh
    if [ $? -ne 0 ]; then
        echo "❌ Network setup failed."
        exit 1
    fi
fi

# Prepare logs
mkdir -p logs
rm -f logs/*.log

echo ""
echo "📝 Logs will be in logs/node_X.log"

# Start nodes
echo ""
echo "🚀 Starting 4 nodes..."

STRATEGY=2

ip netns exec node1 ./build/tdma_node 1 4 $STRATEGY > logs/node_1.log 2>&1 &
PID1=$!
echo "   [+] Node 1 started (PID $PID1)"

ip netns exec node2 ./build/tdma_node 2 4 $STRATEGY > logs/node_2.log 2>&1 &
PID2=$!
echo "   [+] Node 2 started (PID $PID2)"

ip netns exec node3 ./build/tdma_node 3 4 $STRATEGY > logs/node_3.log 2>&1 &
PID3=$!
echo "   [+] Node 3 started (PID $PID3)"

ip netns exec node4 ./build/tdma_node 4 4 $STRATEGY > logs/node_4.log 2>&1 &
PID4=$!
echo "   [+] Node 4 started (PID $PID4)"

sleep 2

echo ""
echo "✅ Network is RUNNING!"
echo ""
echo "📊 Live monitoring:"
echo "   - Tail all logs:     tail -f logs/*.log"
echo "   - Node 1 only:       tail -f logs/node_1.log"
echo ""
echo "Press [ENTER] to stop the network..."
read

echo ""
echo "🛑 Stopping nodes..."
kill $PID1 $PID2 $PID3 $PID4 2>/dev/null
wait $PID1 $PID2 $PID3 $PID4 2>/dev/null

echo "✅ All nodes stopped."
echo ""
echo "To view logs:"
echo "   cat logs/node_1.log"
