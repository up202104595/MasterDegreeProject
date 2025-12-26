#!/bin/bash
# scripts/test_basic.sh
# Simplified test without streaming commands

if [ "$EUID" -ne 0 ]; then
    echo "❌ Please run as root (sudo)"
    exit 1
fi

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

echo "╔════════════════════════════════════════════════╗"
echo "║  BASIC TDMA NETWORK TEST                       ║"
echo "╚════════════════════════════════════════════════╝"
echo ""

# Cleanup
echo "🧹 Cleaning up..."
killall tdma_node 2>/dev/null
sleep 1

for i in 1 2 3 4; do
    ip netns exec node$i iptables -F INPUT 2>/dev/null
    ip netns exec node$i iptables -F OUTPUT 2>/dev/null
done

# Check network
if ! ip netns list | grep -q "node1"; then
    echo "❌ Network namespaces not found!"
    echo "Run: sudo ./scripts/setup_network.sh"
    exit 1
fi

# ========================================
# PHASE 1: Start All Nodes
# ========================================

echo ""
echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}  PHASE 1: Starting All 4 Nodes                 ${NC}"
echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

mkdir -p logs
rm -f logs/*.log

echo ""
echo "🚀 Starting nodes..."

ip netns exec node1 ./build/tdma_node 1 4 2 > logs/node_1.log 2>&1 &
PID1=$!
sleep 0.5

ip netns exec node2 ./build/tdma_node 2 4 2 > logs/node_2.log 2>&1 &
PID2=$!
sleep 0.5

ip netns exec node3 ./build/tdma_node 3 4 2 > logs/node_3.log 2>&1 &
PID3=$!
sleep 0.5

ip netns exec node4 ./build/tdma_node 4 4 2 > logs/node_4.log 2>&1 &
PID4=$!

echo "   Node 1: PID $PID1"
echo "   Node 2: PID $PID2"
echo "   Node 3: PID $PID3"
echo "   Node 4: PID $PID4"

echo ""
echo -e "${YELLOW}⏳ Waiting for initialization (15 seconds)...${NC}"

for i in 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1; do
    echo -ne "   $i seconds...\r"
    sleep 1
done
echo ""

# Check if all alive
ALL_ALIVE=true
for pid in $PID1 $PID2 $PID3 $PID4; do
    if ! kill -0 $pid 2>/dev/null; then
        echo "❌ Process $pid died!"
        ALL_ALIVE=false
    fi
done

if [ "$ALL_ALIVE" = false ]; then
    echo ""
    echo "📝 Check logs for errors:"
    for i in 1 2 3 4; do
        echo ""
        echo "=== Node $i ==="
        tail -20 logs/node_$i.log
    done
    exit 1
fi

echo "✅ All nodes running"

# ========================================
# PHASE 2: Check Heartbeats
# ========================================

echo ""
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${CYAN}  PHASE 2: Verify Heartbeat Exchange            ${NC}"
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

echo ""
sleep 3

for i in 1 2 3 4; do
    HB_SENT=$(grep -c "Heartbeat" logs/node_$i.log 2>/dev/null || echo "0")
    echo "   Node $i: Heartbeats in log: $HB_SENT"
done

# ========================================
# PHASE 3: Check Connectivity
# ========================================

echo ""
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}  PHASE 3: Test IP Connectivity                 ${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

echo ""
echo "🔍 Testing Node 1 → Node 4 connectivity:"

if ip netns exec node1 ping -c 3 -W 2 192.168.2.14 > /dev/null 2>&1; then
    echo -e "${GREEN}✅ Node 1 → Node 4: REACHABLE${NC}"
else
    echo -e "${RED}❌ Node 1 → Node 4: NOT REACHABLE${NC}"
    echo "   Check routing tables:"
    ip netns exec node1 ip route show dev veth1
fi

# ========================================
# PHASE 4: Setup Diamond Topology
# ========================================

echo ""
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}  PHASE 4: Configure Diamond Topology          ${NC}"
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

echo ""
echo "🔧 Creating diamond topology:"
echo ""
echo "       Node2"
echo "      /     \\"
echo "  Node1     Node4"
echo "      \\     /"
echo "       Node3"
echo ""

# Node 1: can reach 2 and 3, not 4
ip netns exec node1 iptables -A OUTPUT -d 192.168.2.14 -j DROP
ip netns exec node1 iptables -A INPUT -s 192.168.2.14 -j DROP

# Node 2: can reach 1 and 4, not 3
ip netns exec node2 iptables -A OUTPUT -d 192.168.2.13 -j DROP
ip netns exec node2 iptables -A INPUT -s 192.168.2.13 -j DROP

# Node 3: can reach 1 and 4, not 2
ip netns exec node3 iptables -A OUTPUT -d 192.168.2.12 -j DROP
ip netns exec node3 iptables -A INPUT -s 192.168.2.12 -j DROP

# Node 4: can reach 2 and 3, not 1
ip netns exec node4 iptables -A OUTPUT -d 192.168.2.11 -j DROP
ip netns exec node4 iptables -A INPUT -s 192.168.2.11 -j DROP

echo "✅ Diamond topology configured"

echo ""
echo -e "${YELLOW}⏳ Waiting for topology refinement (10 seconds)...${NC}"

for i in 10 9 8 7 6 5 4 3 2 1; do
    echo -ne "   $i seconds...\r"
    sleep 1
done
echo ""

# ========================================
# PHASE 5: Verify Routes
# ========================================

echo ""
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${CYAN}  PHASE 5: Verify Routing                       ${NC}"
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

echo ""
echo "📊 Node 1 routes:"
ip netns exec node1 ip route show dev veth1 | grep "192.168.2"

echo ""
echo "🔍 Testing Node 1 → Node 4 (should work via Node 2 or 3):"

if ip netns exec node1 ping -c 3 -W 2 192.168.2.14 > /dev/null 2>&1; then
    echo -e "${GREEN}✅ Node 1 → Node 4: REACHABLE (via intermediate)${NC}"
    ROUTE=$(ip netns exec node1 ip route get 192.168.2.14 | grep via | awk '{print $3}')
    echo "   Route via: $ROUTE"
else
    echo -e "${RED}❌ Node 1 → Node 4: NOT REACHABLE${NC}"
fi

# ========================================
# PHASE 6: Break Link and Test Recovery
# ========================================

echo ""
read -p "Press [ENTER] to test link failure recovery..."

echo ""
echo -e "${RED}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${RED}  PHASE 6: Link Failure Test                    ${NC}"
echo -e "${RED}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

echo ""
echo "🔥 Breaking Node 1 <-> Node 2 link..."

ip netns exec node1 iptables -A OUTPUT -d 192.168.2.12 -j DROP
ip netns exec node1 iptables -A INPUT -s 192.168.2.12 -j DROP
ip netns exec node2 iptables -A OUTPUT -d 192.168.2.11 -j DROP
ip netns exec node2 iptables -A INPUT -s 192.168.2.11 -j DROP

echo "✅ Link broken"

echo ""
echo -e "${YELLOW}⏳ Waiting for timeout detection (10 seconds)...${NC}"

for i in 10 9 8 7 6 5 4 3 2 1; do
    echo -ne "   $i seconds...\r"
    sleep 1
done
echo ""

echo ""
echo "📝 Node 1 timeout detection:"
grep -E "TIMEOUT.*Node 2" logs/node_1.log | tail -3

echo ""
echo "📊 New routes:"
ip netns exec node1 ip route show dev veth1 | grep "192.168.2.14"

echo ""
echo "🔍 Testing Node 1 → Node 4 (should reroute via Node 3):"

if ip netns exec node1 ping -c 3 -W 2 192.168.2.14 > /dev/null 2>&1; then
    echo -e "${GREEN}✅ RECOVERY SUCCESS! Node 1 → Node 4 still reachable${NC}"
    NEW_ROUTE=$(ip netns exec node1 ip route get 192.168.2.14 | grep via | awk '{print $3}')
    echo "   New route via: $NEW_ROUTE"
else
    echo -e "${RED}❌ Recovery failed${NC}"
fi

# ========================================
# Summary
# ========================================

echo ""
echo "╔════════════════════════════════════════════════╗"
echo "║  TEST SUMMARY                                  ║"
echo "╚════════════════════════════════════════════════╝"
echo ""

echo "📊 Statistics:"
for i in 1 2 3 4; do
    TIMEOUTS=$(grep -c "TIMEOUT" logs/node_$i.log 2>/dev/null || echo "0")
    ROUTING=$(grep -c "Routing changed" logs/node_$i.log 2>/dev/null || echo "0")
    echo "   Node $i: Timeouts=$TIMEOUTS, Route Changes=$ROUTING"
done

echo ""
echo "📝 Full logs available in logs/node_*.log"
echo ""

read -p "Stop all nodes? (y/n) " -n 1 -r
echo ""

if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "🛑 Stopping nodes..."
    kill $PID1 $PID2 $PID3 $PID4 2>/dev/null
    wait
    
    echo ""
    echo "🧹 Cleaning iptables..."
    for i in 1 2 3 4; do
        ip netns exec node$i iptables -F INPUT 2>/dev/null
        ip netns exec node$i iptables -F OUTPUT 2>/dev/null
    done
    
    echo "✅ Cleanup complete"
fi

echo ""
echo "Done! 🎯"