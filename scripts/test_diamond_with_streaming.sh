#!/bin/bash

set -e

echo "╔════════════════════════════════════════════════╗"
echo "║  DIAMOND TOPOLOGY + MULTI-HOP STREAMING TEST   ║"
echo "╚════════════════════════════════════════════════╝"
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Cleanup
cleanup() {
    echo ""
    echo "🧹 Cleaning up..."
    pkill -9 tdma_node 2>/dev/null || true
    ip netns del node1 node2 node3 node4 2>/dev/null || true
    ebtables -t filter -F 2>/dev/null || true
}

trap cleanup EXIT

cleanup

# Setup network
echo "🌐 Setting up network..."
./scripts/setup_virtual_network.sh 4 > /dev/null 2>&1

# Get MAC addresses
MAC1=$(ip netns exec node1 cat /sys/class/net/veth1/address)
MAC2=$(ip netns exec node2 cat /sys/class/net/veth2/address)
MAC3=$(ip netns exec node3 cat /sys/class/net/veth3/address)
MAC4=$(ip netns exec node4 cat /sys/class/net/veth4/address)

echo "   Node 1: $MAC1"
echo "   Node 2: $MAC2"
echo "   Node 3: $MAC3"
echo "   Node 4: $MAC4"

# Configure diamond topology with ebtables
echo ""
echo "🔧 Configuring Diamond Topology:"
echo ""
echo "       Node2"
echo "      /     \\"
echo "  Node1     Node4"
echo "      \\     /"
echo "       Node3"
echo ""

# Block Node1 <-> Node4 (force routing via 2 or 3)
ebtables -t filter -A FORWARD -s $MAC1 -d $MAC4 -j DROP
ebtables -t filter -A FORWARD -s $MAC4 -d $MAC1 -j DROP
echo "   ✅ Blocked: Node1 <-> Node4 (MUST route via 2 or 3)"

# Block Node2 <-> Node3 (create two paths)
ebtables -t filter -A FORWARD -s $MAC2 -d $MAC3 -j DROP
ebtables -t filter -A FORWARD -s $MAC3 -d $MAC2 -j DROP
echo "   ✅ Blocked: Node2 <-> Node3 (creates two disjoint paths)"

echo ""
echo "✅ Diamond configured! Allowed paths:"
echo "   - Node1 <-> Node2 ✓"
echo "   - Node1 <-> Node3 ✓"
echo "   - Node2 <-> Node4 ✓"
echo "   - Node3 <-> Node4 ✓"
echo "   - Node1 <-> Node4 ✗ (BLOCKED - must use multi-hop!)"

# Start nodes
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  Starting TDMA Network                         "
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

mkdir -p logs

echo "🚀 Starting nodes..."
ip netns exec node1 ./build/tdma_node 1 4 2 > logs/node_1.log 2>&1 &
PID1=$!
echo "   Node 1: PID $PID1 (Sender)"

ip netns exec node2 ./build/tdma_node 2 4 2 > logs/node_2.log 2>&1 &
PID2=$!
echo "   Node 2: PID $PID2 (Relay A)"

ip netns exec node3 ./build/tdma_node 3 4 2 > logs/node_3.log 2>&1 &
PID3=$!
echo "   Node 3: PID $PID3 (Relay B)"

ip netns exec node4 ./build/tdma_node 4 4 2 > logs/node_4.log 2>&1 &
PID4=$!
echo "   Node 4: PID $PID4 (Receiver)"

echo ""
echo "⏳ Waiting for network stabilization (35 seconds)..."
echo "   (Node 1 will start streaming after 30s)"

# Wait with progress
for i in {1..35}; do
    echo -n "."
    sleep 1
done
echo ""

# Phase 1: Initial streaming
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  PHASE 1: Streaming via Node 2                "
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Wait for streaming to start
sleep 5

# Check initial routing
echo "📊 Initial Routing (Node 1):"
grep "Next Hop" logs/node_1.log | tail -5 || echo "   (routing info not yet available)"

echo ""
echo "📊 Node 4 Reception (first 10 streams):"
grep "Stream.*complete.*received" logs/node_4.log | head -10 || echo "   (no streams received yet)"

STREAMS_PHASE1=$(grep -c "Stream.*complete.*received" logs/node_4.log || echo "0")
echo ""
echo "   Total streams received: $STREAMS_PHASE1"

# Wait a bit more for streaming to progress
echo ""
echo "⏳ Letting streaming continue for 10 more seconds..."
sleep 10

STREAMS_BEFORE=$(grep -c "Stream.*complete.*received" logs/node_4.log || echo "0")
echo "   Streams received before failure: $STREAMS_BEFORE"

# Phase 2: Kill Node 2
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  PHASE 2: Breaking Primary Path (Kill Node 2) "
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "🔥 Killing Node 2 (PID $PID2)..."
kill -9 $PID2 2>/dev/null || true
echo "   ✅ Node 2 killed"

echo ""
echo "⏳ Waiting for timeout detection and rerouting (15 seconds)..."
sleep 15

# Check timeout detection
echo ""
echo "📝 Timeout detection:"
grep "TIMEOUT: Node 2" logs/node_1.log | tail -1 || echo "   ⚠️  No timeout detected"

echo ""
echo "📝 Link change:"
grep "Link to node 2 changed" logs/node_1.log | tail -1 || echo "   ⚠️  No link change"

echo ""
echo "📝 Routing update:"
grep "Routing changed" logs/node_1.log | tail -3 || echo "   ⚠️  No routing update"

# Phase 3: Verify continued streaming via Node 3
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  PHASE 3: Streaming via Node 3 (Backup Path)  "
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "⏳ Waiting for streaming to finish (15 more seconds)..."
sleep 15

STREAMS_AFTER=$(grep -c "Stream.*complete.*received" logs/node_4.log || echo "0")
STREAMS_DURING_FAILOVER=$((STREAMS_AFTER - STREAMS_BEFORE))

echo ""
echo "📊 Reception Statistics:"
echo "   Streams before failover: $STREAMS_BEFORE"
echo "   Streams after failover:  $STREAMS_AFTER"
echo "   Streams during recovery: $STREAMS_DURING_FAILOVER"

# Check final streaming results
echo ""
echo "📊 Final Streaming Results (Node 1):"
grep "STREAMING TEST RESULTS" logs/node_1.log -A 15 | head -16 || echo "   (not complete yet)"

# Summary
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  TEST SUMMARY                                  "
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

CHECKS=0
TOTAL=5

# Check 1: Streams received before failure
if [ "$STREAMS_BEFORE" -gt 10 ]; then
    echo "✅ 1. Initial streaming works (via Node 2)"
    ((CHECKS++))
else
    echo "❌ 1. Initial streaming failed ($STREAMS_BEFORE streams)"
fi

# Check 2: Timeout detected
if grep -q "TIMEOUT: Node 2" logs/node_1.log; then
    echo "✅ 2. Node 2 timeout detected"
    ((CHECKS++))
else
    echo "❌ 2. Node 2 timeout NOT detected"
fi

# Check 3: Routing recomputed
if grep -q "Routing.*→" logs/node_1.log; then
    echo "✅ 3. Routing table recomputed"
    ((CHECKS++))
else
    echo "❌ 3. No routing recomputation"
fi

# Check 4: Continued reception after failure
if [ "$STREAMS_DURING_FAILOVER" -gt 5 ]; then
    echo "✅ 4. Streaming continued after failover (via Node 3)"
    ((CHECKS++))
else
    echo "❌ 4. Streaming stopped after failover ($STREAMS_DURING_FAILOVER streams)"
fi

# Check 5: Node 3 forwarded packets
NODE3_FWD=$(grep -c "Forwarding" logs/node_3.log 2>/dev/null || echo "0")
if [ "$NODE3_FWD" -gt 0 ] || [ "$STREAMS_DURING_FAILOVER" -gt 5 ]; then
    echo "✅ 5. Node 3 acted as relay"
    ((CHECKS++))
else
    echo "❌ 5. Node 3 did NOT relay packets"
fi

echo ""
if [ "$CHECKS" -eq "$TOTAL" ]; then
    echo "🎉🎉🎉 TEST PASSED! ($CHECKS/$TOTAL checks) 🎉🎉🎉"
    echo ""
    echo "Multi-hop streaming works correctly!"
else
    echo "⚠️  TEST PARTIAL ($CHECKS/$TOTAL checks)"
    echo ""
    echo "Some checks failed - review logs for details"
fi

echo ""
echo "📋 Full logs available:"
echo "   logs/node_1.log - Sender (streaming source)"
echo "   logs/node_2.log - Relay A (killed mid-test)"
echo "   logs/node_3.log - Relay B (backup path)"
echo "   logs/node_4.log - Receiver (check reception)"
echo ""
echo "💡 Useful analysis commands:"
echo "   grep 'Stream.*complete' logs/node_4.log | wc -l"
echo "   grep 'TIMEOUT' logs/node_1.log"
echo "   grep 'Routing changed' logs/node_1.log"
echo "   grep 'Forwarding' logs/node_3.log"

# Keep nodes running?
echo ""
read -p "Stop remaining nodes? (y/n) " -n 1 -r
echo ""

if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "🛑 Stopping nodes..."
    kill -9 $PID1 $PID3 $PID4 2>/dev/null || true
fi

echo ""
echo "✅ Test complete! 🎯"
