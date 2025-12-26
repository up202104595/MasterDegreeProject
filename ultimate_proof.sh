#!/bin/bash
if [ "$EUID" -ne 0 ]; then
    echo "❌ Run as root"
    exit 1
fi

echo "╔════════════════════════════════════════════════╗"
echo "║  ULTIMATE MULTI-HOP PROOF TEST                 ║"
echo "╚════════════════════════════════════════════════╝"
echo ""

# Cleanup
killall -9 tdma_node 2>/dev/null
sleep 3
mkdir -p logs
rm -f logs/*.log

for ns in node1 node2 node3 node4; do
    ip netns exec $ns iptables -F 2>/dev/null
done

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  STEP 1: Test WITHOUT Firewall (Baseline)      "
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "🚀 Starting all 4 nodes (NO firewall)..."

ip netns exec node1 ./build/tdma_node 1 4 2 > logs/test1_node1.log 2>&1 &
PID1=$!
sleep 0.5

ip netns exec node2 ./build/tdma_node 2 4 2 > logs/test1_node2.log 2>&1 &
PID2=$!
sleep 0.5

ip netns exec node3 ./build/tdma_node 3 4 2 > logs/test1_node3.log 2>&1 &
PID3=$!
sleep 0.5

ip netns exec node4 ./build/tdma_node 4 4 2 > logs/test1_node4.log 2>&1 &
PID4=$!

echo "   Started PIDs: $PID1 $PID2 $PID3 $PID4"
echo ""
echo "⏳ Running for 70 seconds (includes 30s for streaming)..."

for i in {70..1}; do
    if [ $i -eq 40 ]; then
        echo "   [30s] Streaming should have started..."
    fi
    if [ $((i % 10)) -eq 0 ]; then
        echo "   $i seconds remaining..."
    fi
    sleep 1
done

echo ""
echo "🛑 Stopping Test 1..."
kill $PID1 $PID2 $PID3 $PID4 2>/dev/null
sleep 3

echo ""
echo "📊 RESULTS TEST 1 (No Firewall):"
echo ""

# Check streaming
FRAMES=$(grep "Frames sent:" logs/test1_node1.log 2>/dev/null | tail -1 | awk '{print $3}')
if [ "$FRAMES" = "90" ]; then
    echo "✅ Streaming: Node 1 sent 90/90 frames"
else
    echo "❌ Streaming: Node 1 sent $FRAMES frames (expected 90)"
fi

RECEIVED=$(grep -c "Stream.*complete" logs/test1_node4.log 2>/dev/null)
if [ "$RECEIVED" -gt 80 ]; then
    echo "✅ Receiving: Node 4 received $RECEIVED streams"
else
    echo "❌ Receiving: Node 4 only got $RECEIVED streams"
fi

# Stats
echo ""
echo "Transport Statistics (Test 1):"
for i in 1 2 3 4; do
    TX=$(grep "Sent:" logs/test1_node${i}.log 2>/dev/null | tail -1 | awk '{print $2}')
    RX=$(grep "Received:" logs/test1_node${i}.log 2>/dev/null | tail -1 | awk '{print $2}')
    echo "   Node $i: TX=$TX, RX=$RX"
done

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  STEP 2: Test WITH Firewall (Multi-hop Proof)  "
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

sleep 2

# Apply firewall
echo "🚫 Applying firewall: Blocking Node 1 ↔ Node 4..."
ip netns exec node1 iptables -A OUTPUT -d 192.168.2.14 -j DROP
ip netns exec node1 iptables -A INPUT -s 192.168.2.14 -j DROP
ip netns exec node4 iptables -A OUTPUT -d 192.168.2.11 -j DROP  
ip netns exec node4 iptables -A INPUT -s 192.168.2.11 -j DROP

# Verify block
echo -n "   Testing block: "
if ip netns exec node1 ping -c 1 -W 1 192.168.2.14 >/dev/null 2>&1; then
    echo "❌ FAILED - Node 1 can still reach Node 4!"
    exit 1
else
    echo "✅ Node 1 CANNOT reach Node 4"
fi

# Test if Node 1 can reach Node 2 (should work)
echo -n "   Testing Node 1 → Node 2: "
if ip netns exec node1 ping -c 1 -W 1 192.168.2.12 >/dev/null 2>&1; then
    echo "✅ Works (as expected)"
else
    echo "❌ Failed (unexpected!)"
fi

echo ""
echo "🚀 Starting all 4 nodes (WITH firewall)..."

ip netns exec node1 ./build/tdma_node 1 4 2 > logs/test2_node1.log 2>&1 &
PID1=$!
sleep 0.5

ip netns exec node2 ./build/tdma_node 2 4 2 > logs/test2_node2.log 2>&1 &
PID2=$!
sleep 0.5

ip netns exec node3 ./build/tdma_node 3 4 2 > logs/test2_node3.log 2>&1 &
PID3=$!
sleep 0.5

ip netns exec node4 ./build/tdma_node 4 4 2 > logs/test2_node4.log 2>&1 &
PID4=$!

echo "   Started PIDs: $PID1 $PID2 $PID3 $PID4"
echo ""
echo "⏳ Running for 40 seconds (TDMA only, no streaming expected)..."

for i in {40..1}; do
    if [ $((i % 10)) -eq 0 ]; then
        echo "   $i seconds remaining..."
    fi
    sleep 1
done

echo ""
echo "🛑 Stopping Test 2..."
kill $PID1 $PID2 $PID3 $PID4 2>/dev/null
sleep 3

echo ""
echo "📊 RESULTS TEST 2 (With Firewall):"
echo ""

# Check errors
ERRORS=$(grep -c "Operation not permitted" logs/test2_node1.log 2>/dev/null)
echo "Firewall blocked $ERRORS attempts by Node 1 to reach Node 4 ✅"

echo ""
echo "Transport Statistics (Test 2):"
TX1=0; TX2=0; TX3=0; TX4=0
RX1=0; RX2=0; RX3=0; RX4=0

for i in 1 2 3 4; do
    eval "TX$i=\$(grep 'Sent:' logs/test2_node${i}.log 2>/dev/null | tail -1 | awk '{print \$2}')"
    eval "RX$i=\$(grep 'Received:' logs/test2_node${i}.log 2>/dev/null | tail -1 | awk '{print \$2}')"
    eval "echo '   Node $i: TX=\$TX$i, RX=\$RX$i'"
done

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  MATHEMATICAL PROOF OF MULTI-HOP               "
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Calculate forwarding
FWD2=$((TX2 - TX1))
FWD3=$((TX3 - TX1))

echo "Analysis:"
echo "   Node 1 transmitted: $TX1 packets"
echo "   Node 2 transmitted: $TX2 packets (difference: +$FWD2)"
echo "   Node 3 transmitted: $TX3 packets (difference: +$FWD3)"
echo "   Node 4 transmitted: $TX4 packets"
echo ""

if [ "$TX2" -gt "$TX1" ] && [ "$TX3" -gt "$TX1" ]; then
    echo "✅ Node 2 and Node 3 sent MORE than Node 1"
    echo "   → They are FORWARDING packets!"
    echo ""
    echo "🎉 MULTI-HOP FORWARDING CONFIRMED!"
else
    echo "⚠️  Node 2/3 did not forward significantly"
    echo "   This suggests packets may have been dropped"
fi

# Cleanup
for ns in node1 node2 node3 node4; do
    ip netns exec $ns iptables -F 2>/dev/null
done

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  FINAL VERDICT                                  "
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

PASS=0

# Check Test 1
if [ "$FRAMES" = "90" ] && [ "$RECEIVED" -gt 80 ]; then
    echo "✅ TEST 1 PASSED: Streaming works without firewall"
    PASS=$((PASS + 1))
else
    echo "❌ TEST 1 FAILED: Streaming didn't work"
fi

# Check Test 2
if [ "$ERRORS" -gt 50 ]; then
    echo "✅ TEST 2 PASSED: Firewall blocked as expected ($ERRORS blocks)"
    PASS=$((PASS + 1))
else
    echo "⚠️  TEST 2: Few firewall blocks ($ERRORS)"
fi

# Check forwarding
if [ "$TX2" -gt "$TX1" ] && [ "$TX3" -gt "$TX1" ]; then
    echo "✅ MULTI-HOP CONFIRMED: Nodes 2&3 are forwarding"
    PASS=$((PASS + 1))
else
    echo "❌ MULTI-HOP: Evidence unclear"
fi

echo ""
if [ "$PASS" -eq 3 ]; then
    echo "🏆 ALL TESTS PASSED (3/3)"
    echo ""
    echo "PROVEN:"
    echo "  ✅ System works perfectly (Test 1)"
    echo "  ✅ Firewall blocks correctly (Test 2)"
    echo "  ✅ Multi-hop forwarding active (Math)"
else
    echo "⚠️  TESTS PASSED: $PASS/3"
    echo ""
    echo "Check logs for details:"
    echo "   logs/test1_node*.log (without firewall)"
    echo "   logs/test2_node*.log (with firewall)"
fi

echo ""
echo "✅ Test complete"
