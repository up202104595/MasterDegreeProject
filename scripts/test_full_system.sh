#!/bin/bash
# test_full_system.sh - FIXED VERSION
# Complete system test with correct log parsing

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
    # FIXED: Correct grep pattern
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

# Wait for streaming to complete
echo "⏳ Waiting 15 more seconds for streaming..."
sleep 15

echo "📹 Streaming Activity:"

# Check sender
if grep -q "Video streaming test PASSED" logs/node_1.log; then
    echo "   ✅ Node 1 (Sender): Streaming completed successfully"
    grep "Frames sent:" logs/node_1.log | tail -1 | sed 's/^/      /'
else
    echo "   ⚠️  Node 1: Streaming not completed"
fi

# Check receiver
if grep -q "Stream.*complete" logs/node_4.log; then
    echo "   ✅ Node 4 (Receiver): Streams received"
    grep "Stream.*complete" logs/node_4.log | tail -1 | sed 's/^/      /'
else
    echo "   ⚠️  Node 4: No streams received"
fi

# PHASE 4: Failure Test
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  PHASE 4: Link Failure & Recovery Test         "
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

read -p "Press [ENTER] to kill Node 2 and test recovery..."
echo ""

echo "🔥 Killing Node 2 (PID $PID2)..."
kill -9 $PID2 2>/dev/null
echo "   ✅ Node 2 killed"

echo ""
echo "⏳ Monitoring timeout detection (15 seconds)..."

for i in {15..1}; do
    echo -ne "   $i seconds...\r"
    if grep -q "TIMEOUT.*Node 2" logs/node_1.log 2>/dev/null; then
        echo ""
        echo "   ✅ Timeout detected!"
        break
    fi
    sleep 1
done
echo ""

# Summary
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  FINAL SUMMARY                                  "
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

TESTS_PASSED=0

# Check heartbeats (FIXED)
SENT=$(grep "Heartbeats sent:" logs/node_1.log 2>/dev/null | tail -1 | awk '{print $3}')
if [ -n "$SENT" ] && [ "$SENT" -gt 0 ]; then
    echo "✅ 1. TDMA heartbeats working ($SENT sent)"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    echo "❌ 1. TDMA heartbeats FAILED"
fi

if grep -q "Video streaming test PASSED" logs/node_1.log; then
    echo "✅ 2. Data streaming working"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    echo "❌ 2. Data streaming FAILED"
fi

if grep -q "TIMEOUT.*Node 2" logs/node_1.log; then
    echo "✅ 3. Failure detection working"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    echo "❌ 3. Failure detection not confirmed"
fi

echo ""
if [ $TESTS_PASSED -eq 3 ]; then
    echo "🎉 SYSTEM TEST PASSED (3/3) - PERFECT!"
elif [ $TESTS_PASSED -ge 2 ]; then
    echo "✅ SYSTEM TEST PASSED ($TESTS_PASSED/3)"
else
    echo "⚠️  SYSTEM TEST INCOMPLETE ($TESTS_PASSED/3)"
fi

echo ""
echo "📋 Logs: logs/node_X.log"
echo ""

# Show performance metrics
if grep -q "Recomputation Timing" logs/node_1.log; then
    echo "📊 Performance Metrics:"
    grep "Average:" logs/node_1.log | tail -1 | sed 's/^/   /'
    grep "Status:" logs/node_1.log | tail -1 | sed 's/^/   /'
    echo ""
fi

read -p "Stop remaining nodes? (y/n) " -n 1 -r
echo ""

if [[ $REPLY =~ ^[Yy]$ ]]; then
    kill $PID1 $PID3 $PID4 2>/dev/null
    echo "✅ Stopped"
fi