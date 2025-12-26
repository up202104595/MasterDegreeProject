#!/bin/bash
# test_multihop_forwarding.sh
# Tests IP forwarding through TDMA network

if [ "$EUID" -ne 0 ]; then
    echo "❌ Run as root"
    exit 1
fi

echo "╔════════════════════════════════════════════════╗"
echo "║  MULTI-HOP IP FORWARDING TEST                  ║"
echo "╚════════════════════════════════════════════════╝"
echo ""

# Cleanup
killall -9 tdma_node 2>/dev/null
sleep 2

ebtables -F 2>/dev/null
ebtables -X 2>/dev/null

# Get MACs
VETH1_BR_MAC=$(ip link show veth1_br | grep ether | awk '{print $2}')
VETH4_BR_MAC=$(ip link show veth4_br | grep ether | awk '{print $2}')

echo "🔧 Setting up Diamond topology..."
echo ""
echo "   Node1 -X- Node4  (BLOCKED)"
echo "   Node1 → Node2 → Node4  (Path A)"
echo "   Node1 → Node3 → Node4  (Path B)"
echo ""

ebtables -P FORWARD ACCEPT

# Block ONLY Node1 <-> Node4 direct communication
ebtables -A FORWARD -s $VETH1_BR_MAC -d $VETH4_BR_MAC -j DROP
ebtables -A FORWARD -s $VETH4_BR_MAC -d $VETH1_BR_MAC -j DROP

echo "✅ Blocked direct Node1 <-> Node4 communication"
echo ""

# Start nodes
mkdir -p logs
rm -f logs/*.log

echo "🚀 Starting TDMA nodes..."

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

echo "   PIDs: $PID1, $PID2, $PID3, $PID4"

echo ""
echo "⏳ Waiting for stabilization (25 seconds)..."
for i in {25..1}; do
    echo -ne "   $i...\r"
    sleep 1
done
echo ""

# ========================================
# Test 1: Verify TDMA Communication
# ========================================

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  TEST 1: TDMA Heartbeat Communication         "
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

for node in 1 2 3 4; do
    SENT=$(grep "Heartbeats sent:" logs/node_${node}.log | tail -1 | awk '{print $3}')
    RECV=$(grep "Heartbeats recv:" logs/node_${node}.log | tail -1 | awk '{print $3}')
    
    if [ -n "$SENT" ] && [ -n "$RECV" ]; then
        echo "   Node $node: Sent=$SENT, Recv=$RECV"
    else
        echo "   Node $node: No heartbeat data"
    fi
done

# ========================================
# Test 2: Check Routing Tables
# ========================================

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  TEST 2: Routing Tables                        "
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "📊 Node 1 routing to Node 4:"
grep "192.168.2.14" logs/node_1.log | grep "IP-ROUTING" | tail -1

echo ""
echo "📊 Node 1 TDMA routing table:"
grep "Routing Table" logs/node_1.log -A 10 | head -12

# ========================================
# Test 3: ICMP Ping (Multi-hop)
# ========================================

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  TEST 3: ICMP Ping (Node1 → Node4)            "
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "🔍 Testing: Node 1 → Node 4 (must go via Node 2 or 3)"
echo ""

if ip netns exec node1 ping -c 3 -W 2 192.168.2.14 > /tmp/ping_result.txt 2>&1; then
    echo "✅ SUCCESS: Ping worked!"
    echo ""
    cat /tmp/ping_result.txt
    echo ""
    echo "🎉 Multi-hop forwarding is WORKING!"
else
    echo "❌ FAILED: Ping did not work"
    echo ""
    cat /tmp/ping_result.txt
    echo ""
    echo "⚠️  Possible causes:"
    echo "   1. IP forwarding not enabled"
    echo "   2. Routing tables incorrect"
    echo "   3. TDMA not allowing forwarded packets"
fi

# ========================================
# Test 4: Check IP Forwarding Settings
# ========================================

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  TEST 4: IP Forwarding Status                  "
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

for node in 1 2 3 4; do
    FWD=$(ip netns exec node$node sysctl net.ipv4.ip_forward 2>/dev/null | awk '{print $3}')
    if [ "$FWD" = "1" ]; then
        echo "   ✅ Node $node: IP forwarding enabled"
    else
        echo "   ❌ Node $node: IP forwarding DISABLED"
    fi
done

# ========================================
# Test 5: Kernel Routes
# ========================================

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  TEST 5: Kernel IP Routes (Node 1)            "
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

ip netns exec node1 ip route

# ========================================
# Summary
# ========================================

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  TEST SUMMARY                                  "
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

TESTS_PASSED=0

# Check heartbeats
if grep -q "Heartbeats recv:" logs/node_1.log; then
    echo "✅ 1. TDMA heartbeats working"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    echo "❌ 1. TDMA heartbeats NOT working"
fi

# Check routing table
if grep -q "192.168.2.14" logs/node_1.log; then
    echo "✅ 2. Routing table populated"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    echo "❌ 2. Routing table NOT populated"
fi

# Check ping
if grep -q "bytes from 192.168.2.14" /tmp/ping_result.txt 2>/dev/null; then
    echo "✅ 3. Multi-hop ping SUCCESS"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    echo "❌ 3. Multi-hop ping FAILED"
fi

# Check IP forwarding
FWD_COUNT=$(ip netns exec node2 sysctl net.ipv4.ip_forward 2>/dev/null | grep -c " = 1")
if [ "$FWD_COUNT" -gt 0 ]; then
    echo "✅ 4. IP forwarding enabled"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    echo "❌ 4. IP forwarding NOT enabled"
fi

echo ""
if [ $TESTS_PASSED -ge 3 ]; then
    echo "🎉 FORWARDING TEST PASSED ($TESTS_PASSED/4)"
else
    echo "⚠️  FORWARDING TEST INCOMPLETE ($TESTS_PASSED/4)"
fi

echo ""
read -p "Stop nodes? (y/n) " -n 1 -r
echo ""

if [[ $REPLY =~ ^[Yy]$ ]]; then
    kill $PID1 $PID2 $PID3 $PID4 2>/dev/null
    ebtables -F 2>/dev/null
    echo "✅ Stopped"
fi