#!/bin/bash
# test_full_system.sh - Complete system test

if [ "$EUID" -ne 0 ]; then
    echo "❌ Run as root"
    exit 1
fi

echo "╔════════════════════════════════════════════════╗"
echo "║  COMPLETE TDMA SYSTEM TEST                     ║"
echo "║  (RA-TDMAs+ | Routing | Streaming | Recovery)  ║"
echo "╚════════════════════════════════════════════════╝"
echo ""

# Cleanup
killall -9 tdma_node 2>/dev/null
sleep 2

mkdir -p logs
rm -f logs/*.log

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  PHASE 1: Network Initialization               "
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "🚀 Starting 4 TDMA nodes..."

ip netns exec node1 ./build/tdma_node 1 4 2 > logs/node_1.log 2>&1 &
PID1=$!
sleep 0.3

ip netns exec node2 ./build/tdma_node 2 4 2 > logs/node_2.log 2>&1 &
PID2=$!
sleep 0.3

ip netns exec node3 ./build/tdma_node 3 4 2 > logs/node_3.log 2>&1 &
PID3=$!
sleep 0.3

ip netns exec node4 ./build/tdma_node 4 4 2 > logs/node_4.log 2>&1 &
PID4=$!

echo "   Node 1: PID $PID1 (Sender)"
echo "   Node 2: PID $PID2"
echo "   Node 3: PID $PID3"
echo "   Node 4: PID $PID4 (Receiver)"

echo ""
echo "⏳ Waiting for network stabilization (40 seconds)..."
echo "   (Node 1 will start streaming after 30s)"
for i in {40..1}; do
    echo -ne "   $i seconds...\r"
    sleep 1
done
echo ""

# PHASE 2: Check TDMA
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  PHASE 2: TDMA Operation Status                "
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "📊 Heartbeat Statistics:"
for node in 1 2 3 4; do
    SENT=$(grep "Heartbeats sent:" logs/node_${node}.log 2>/dev/null | tail -1 | awk '{print $3}')
    RECV=$(grep "Heartbeats recv:" logs/node_${node}.log 2>/dev/null | tail -1 | awk '{print $3}')
    
    if [ -n "$SENT" ] && [ -n "$RECV" ]; then
        echo "   Node $node: TX=$SENT, RX=$RECV"
    else
        echo "   Node $node: No data yet"
    fi
done

# PHASE 3: Check Streaming
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  PHASE 3: Data Streaming Status                "
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "⏳ Waiting 15 more seconds for streaming..."
sleep 15

echo "📹 Streaming Activity:"

# Check sender
if grep -q "Video streaming test PASSED" logs/node_1.log; then
    echo "   ✅ Node 1 (Sender): Streaming completed successfully"
    grep "Frames sent:" logs/node_1.log 2>/dev/null | tail -1 | sed 's/^/      /'
else
    echo "   ⚠️  Node 1: Streaming not completed"
fi

# Check receiver
if grep -q "Stream.*complete" logs/node_4.log; then
    echo "   ✅ Node 4 (Receiver): Streams received"
    grep "Stream" logs/node_4.log 2>/dev/null | grep "complete" | tail -1 | sed 's/^/      /'
else
    echo "   ⚠️  Node 4: No streams received"
fi

# Cleanup
echo ""
echo "🧹 Stopping all nodes..."
killall -9 tdma_node 2>/dev/null

echo ""
echo "✅ Test complete! Check logs/ for details"
echo ""
echo "Quick checks:"
echo "  Sender log:   less logs/node_1.log"
echo "  Receiver log: less logs/node_4.log"
echo "  All logs:     grep -i error logs/*.log"