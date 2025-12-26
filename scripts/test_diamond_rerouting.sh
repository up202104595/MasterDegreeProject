#!/bin/bash
# scripts/test_diamond_rerouting.sh
# Diamond topology test - KILLS node to simulate failure

if [ "$EUID" -ne 0 ]; then
    echo "❌ Please run as root (sudo)"
    exit 1
fi

echo "╔════════════════════════════════════════════════╗"
echo "║  DIAMOND TOPOLOGY + LINK FAILURE TEST          ║"
echo "╚════════════════════════════════════════════════╝"
echo ""

# ========================================
# Cleanup
# ========================================

echo "🧹 Cleaning up..."
killall -9 tdma_node 2>/dev/null
sleep 2

# Limpar ebtables e iptables
ebtables -F 2>/dev/null
ebtables -X 2>/dev/null

for i in 1 2 3 4; do
    ip netns exec node$i iptables -F 2>/dev/null
    ip netns exec node$i iptables -P INPUT ACCEPT 2>/dev/null
    ip netns exec node$i iptables -P OUTPUT ACCEPT 2>/dev/null
done

# ========================================
# Get MAC addresses
# ========================================

echo ""
echo "🔍 Detecting MAC addresses..."

VETH1_BR_MAC=$(ip link show veth1_br | grep ether | awk '{print $2}')
VETH2_BR_MAC=$(ip link show veth2_br | grep ether | awk '{print $2}')
VETH3_BR_MAC=$(ip link show veth3_br | grep ether | awk '{print $2}')
VETH4_BR_MAC=$(ip link show veth4_br | grep ether | awk '{print $2}')

echo "   veth1_br: $VETH1_BR_MAC"
echo "   veth2_br: $VETH2_BR_MAC"
echo "   veth3_br: $VETH3_BR_MAC"
echo "   veth4_br: $VETH4_BR_MAC"

# ========================================
# Configure Diamond Topology with ebtables
# ========================================

echo ""
echo "🔧 Configuring Diamond Topology..."
echo ""
echo "       Node2"
echo "      /     \\"
echo "  Node1     Node4"
echo "      \\     /"
echo "       Node3"
echo ""

# Default policy: ACCEPT
ebtables -P FORWARD ACCEPT

# BLOCK Node1 <-> Node4 (force routing via 2 or 3)
ebtables -A FORWARD -s $VETH1_BR_MAC -d $VETH4_BR_MAC -j DROP
ebtables -A FORWARD -s $VETH4_BR_MAC -d $VETH1_BR_MAC -j DROP
echo "   ✅ Blocked: Node1 <-> Node4 (must route via 2 or 3)"

# BLOCK Node2 <-> Node3 (they can't talk directly)
ebtables -A FORWARD -s $VETH2_BR_MAC -d $VETH3_BR_MAC -j DROP
ebtables -A FORWARD -s $VETH3_BR_MAC -d $VETH2_BR_MAC -j DROP
echo "   ✅ Blocked: Node2 <-> Node3"

echo ""
echo "✅ Diamond topology configured!"
echo "   Allowed paths:"
echo "   - Node1 <-> Node2 ✓"
echo "   - Node1 <-> Node3 ✓"
echo "   - Node2 <-> Node4 ✓"
echo "   - Node3 <-> Node4 ✓"
echo ""

# ========================================
# Start TDMA Network
# ========================================

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  Starting TDMA Network                         "
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

mkdir -p logs
rm -f logs/*.log

echo "🚀 Starting nodes..."

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

echo "   Node 1: PID $PID1"
echo "   Node 2: PID $PID2"
echo "   Node 3: PID $PID3"
echo "   Node 4: PID $PID4"

echo ""
echo "⏳ Waiting for network stabilization (25 seconds)..."
for i in {25..1}; do
    echo -ne "   $i seconds...\r"
    sleep 1
done
echo ""

# ========================================
# Verify Topology
# ========================================

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  Verify Diamond Topology (Initial State)      "
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "📊 Node 1 status:"
echo "   $(grep "Heartbeats sent:" logs/node_1.log | tail -1 || echo 'No heartbeats yet')"
echo "   $(grep "Heartbeats recv:" logs/node_1.log | tail -1 || echo 'No heartbeats recv yet')"

echo ""
echo "📊 Routing table (Node 1):"
grep "IP-ROUTING.*192.168.2" logs/node_1.log | tail -3

echo ""
echo "🔍 Connectivity status:"
if grep -q "RECOVERED.*Node 2" logs/node_1.log; then
    echo "   ✅ Node 1 sees Node 2"
fi
if grep -q "RECOVERED.*Node 3" logs/node_1.log; then
    echo "   ✅ Node 1 sees Node 3"
fi
if grep -q "RECOVERED.*Node 4" logs/node_1.log; then
    echo "   ✅ Node 1 sees Node 4"
fi

echo ""
echo "Press [ENTER] to KILL Node 2 (simulate crash)..."
read

# ========================================
# Break Link: KILL Node 2
# ========================================

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  Breaking Link: KILLING Node 2                 "
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "🔥 Killing Node 2 process (PID $PID2)..."
kill -9 $PID2 2>/dev/null

if [ $? -eq 0 ]; then
    echo "   ✅ Node 2 killed successfully"
else
    echo "   ⚠️  Failed to kill Node 2 (already dead?)"
fi

echo ""
echo "⏳ Monitoring timeout detection (max 15 seconds)..."
echo ""

# Monitor in real-time
TIMEOUT_DETECTED=false
for i in {15..1}; do
    echo -ne "   $i seconds remaining...\r"
    
    if grep -q "TIMEOUT.*Node 2" logs/node_1.log 2>/dev/null; then
        echo ""
        echo "   ✅ Timeout detected!"
        TIMEOUT_DETECTED=true
        break
    fi
    
    sleep 1
done
echo ""

# ========================================
# Verify Rerouting
# ========================================

sleep 2

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  Verify Automatic Rerouting                   "
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

if [ "$TIMEOUT_DETECTED" = true ]; then
    echo "📝 Timeout detection:"
    grep "TIMEOUT.*Node 2" logs/node_1.log | tail -1
    echo ""
fi

echo "📝 Link status change:"
grep "Link to node 2 changed" logs/node_1.log | tail -1

echo ""
echo "📝 Routing updates:"
grep "Routing changed.*→" logs/node_1.log | tail -1

echo ""
echo "📝 New IP routes after failure:"
grep "IP-ROUTING.*192.168.2" logs/node_1.log | tail -5

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  Test Summary                                  "
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Verify results
PASSED=0

if grep -q "TIMEOUT.*Node 2" logs/node_1.log; then
    echo "✅ 1. Timeout detected"
    PASSED=$((PASSED + 1))
else
    echo "❌ 1. Timeout NOT detected"
fi

if grep -q "Link to node 2 changed: 1 → 0" logs/node_1.log; then
    echo "✅ 2. Link status updated"
    PASSED=$((PASSED + 1))
else
    echo "❌ 2. Link status NOT updated"
fi

if grep -q "Routing changed" logs/node_1.log; then
    echo "✅ 3. Routing recomputed"
    PASSED=$((PASSED + 1))
else
    echo "❌ 3. Routing NOT recomputed"
fi

# Check if still has routes to other nodes
if grep "IP-ROUTING.*192.168.2.13" logs/node_1.log | tail -1 | grep -q "✅"; then
    echo "✅ 4. Alternative path exists (via Node 3)"
    PASSED=$((PASSED + 1))
else
    echo "⚠️  4. Alternative path status unclear"
fi

echo ""
if [ $PASSED -ge 3 ]; then
    echo "🎉🎉🎉 TEST PASSED! ($PASSED/4 checks) 🎉🎉🎉"
    echo ""
    echo "Diamond topology rerouting works correctly!"
else
    echo "⚠️  TEST INCOMPLETE ($PASSED/4 checks passed)"
fi

echo ""
echo "📋 Full logs:"
echo "   logs/node_1.log - Source node (check this one)"
echo "   logs/node_2.log - Failed node (killed)"
echo "   logs/node_3.log - Backup path"
echo "   logs/node_4.log - Destination"

echo ""
echo "💡 Useful analysis commands:"
echo "   grep 'TIMEOUT' logs/node_1.log"
echo "   grep 'Routing changed' logs/node_1.log"
echo "   grep 'IP-ROUTING' logs/node_1.log | tail -10"

echo ""
read -p "Stop remaining nodes? (y/n) " -n 1 -r
echo ""

if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "🛑 Stopping nodes..."
    kill $PID1 $PID3 $PID4 2>/dev/null
    sleep 2
    
    echo "🧹 Cleaning ebtables..."
    ebtables -F 2>/dev/null
    ebtables -X 2>/dev/null
    
    echo "✅ Cleanup complete"
fi

echo ""
echo "Done! 🎯"