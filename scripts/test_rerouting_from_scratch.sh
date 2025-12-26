#!/bin/bash
# scripts/test_rerouting_from_scratch.sh
# Bloqueia INPUT + OUTPUT para forçar topologia linear

if [ "$EUID" -ne 0 ]; then
    echo "❌ Please run as root (sudo)"
    exit 1
fi

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo "╔════════════════════════════════════════════════╗"
echo "║  REROUTING TEST (INPUT+OUTPUT blocking)        ║"
echo "╚════════════════════════════════════════════════╝"
echo ""

# ========================================
# Cleanup
# ========================================

echo "🧹 Cleaning up..."
killall tdma_node 2>/dev/null
sleep 2

for i in 1 2 3 4; do
    ip netns exec node$i iptables -F INPUT 2>/dev/null
    ip netns exec node$i iptables -F OUTPUT 2>/dev/null
done

# ========================================
# Setup LINEAR topology (INPUT + OUTPUT)
# ========================================

echo ""
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}  Configuring LINEAR Topology (Bidirectional)  ${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

echo ""
echo "🔧 Blocking INPUT + OUTPUT (not just OUTPUT)..."
echo ""
echo "   Topology: Node1 --- Node2 --- Node3 --- Node4"
echo ""

# Node 1: só comunica com Node 2
ip netns exec node1 iptables -A OUTPUT -d 192.168.2.13 -j DROP
ip netns exec node1 iptables -A OUTPUT -d 192.168.2.14 -j DROP
ip netns exec node1 iptables -A INPUT -s 192.168.2.13 -j DROP
ip netns exec node1 iptables -A INPUT -s 192.168.2.14 -j DROP
echo "   ✅ Node 1: only Node 2"

# Node 2: comunica com Node 1 e Node 3
ip netns exec node2 iptables -A OUTPUT -d 192.168.2.14 -j DROP
ip netns exec node2 iptables -A INPUT -s 192.168.2.14 -j DROP
echo "   ✅ Node 2: only Node 1 and Node 3"

# Node 3: comunica com Node 2 e Node 4
ip netns exec node3 iptables -A OUTPUT -d 192.168.2.11 -j DROP
ip netns exec node3 iptables -A INPUT -s 192.168.2.11 -j DROP
echo "   ✅ Node 3: only Node 2 and Node 4"

# Node 4: só comunica com Node 3
ip netns exec node4 iptables -A OUTPUT -d 192.168.2.11 -j DROP
ip netns exec node4 iptables -A OUTPUT -d 192.168.2.12 -j DROP
ip netns exec node4 iptables -A INPUT -s 192.168.2.11 -j DROP
ip netns exec node4 iptables -A INPUT -s 192.168.2.12 -j DROP
echo "   ✅ Node 4: only Node 3"

echo ""
echo "✅ Bidirectional blocking configured"

# ========================================
# Start TDMA
# ========================================

echo ""
echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}  Starting TDMA Network                         ${NC}"
echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

echo ""
echo "🚀 Starting TDMA nodes..."

mkdir -p logs
rm -f logs/*.log

ip netns exec node1 ./build/tdma_node 1 4 2 > logs/node_1.log 2>&1 &
PID1=$!
ip netns exec node2 ./build/tdma_node 2 4 2 > logs/node_2.log 2>&1 &
PID2=$!
ip netns exec node3 ./build/tdma_node 3 4 2 > logs/node_3.log 2>&1 &
PID3=$!
ip netns exec node4 ./build/tdma_node 4 4 2 > logs/node_4.log 2>&1 &
PID4=$!

echo "   Node 1: PID $PID1"
echo "   Node 2: PID $PID2"
echo "   Node 3: PID $PID3"
echo "   Node 4: PID $PID4"

echo ""
echo -e "${YELLOW}⏳ Waiting for topology detection (20 seconds)...${NC}"
echo "   (Nodes should detect LINEAR topology via timeouts)"

for i in 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1; do
    echo -ne "   $i seconds...\r"
    sleep 1
done
echo ""

# Verifica processos
for pid in $PID1 $PID2 $PID3 $PID4; do
    if ! kill -0 $pid 2>/dev/null; then
        echo "❌ Process $pid died! Check logs."
        exit 1
    fi
done

echo "✅ All nodes running"

# ========================================
# Verify LINEAR topology
# ========================================

echo ""
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}  Verify LINEAR Topology Detection              ${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

echo ""
echo "📝 Node 1 timeouts (should see Node3 and Node4):"
grep "TIMEOUT" logs/node_1.log | tail -5

echo ""
echo "📝 Node 1 routing updates:"
grep "Routing changed" logs/node_1.log | tail -5

echo ""
echo "📝 Node 1 ARP entries:"
grep "ARP.*→ Next Hop" logs/node_1.log | tail -10

echo ""
echo "📊 Node 1 ARP Table:"
ip netns exec node1 arp -n

NODE2_MAC=$(ip netns exec node1 arp -n | grep "192.168.2.12" | awk '{print $3}')
NODE4_ENTRY=$(ip netns exec node1 arp -n | grep "192.168.2.14")
NODE4_MAC=$(echo "$NODE4_ENTRY" | awk '{print $3}')

echo ""
echo "🔍 Analysis:"
echo "   Node 2 MAC: $NODE2_MAC"
echo "   Node 4 MAC: $NODE4_MAC"
echo ""

if [ -n "$NODE4_MAC" ]; then
    if [ "$NODE2_MAC" == "$NODE4_MAC" ]; then
        echo -e "${GREEN}✅ CORRECT!${NC} Node1 → Node4 routes via Node2"
        echo "   Both use same next-hop MAC: $NODE2_MAC"
    else
        echo -e "${YELLOW}⚠️  Node4 has different MAC${NC}"
        echo "   This means direct route (not via Node2)"
    fi
else
    echo -e "${YELLOW}⚠️  Node4 not in ARP table yet${NC}"
fi

echo ""
echo "💾 Saving initial ARP state..."
ip netns exec node1 arp -n > /tmp/arp_linear.txt

echo ""
read -p "Press [ENTER] to break Node2-Node3 link..."

# ========================================
# Break Link Node2-Node3
# ========================================

echo ""
echo -e "${RED}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${RED}  Breaking Link Node2 <-> Node3                 ${NC}"
echo -e "${RED}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

echo ""
echo "🔥 Cutting link between Node 2 and Node 3..."

ip netns exec node2 iptables -A OUTPUT -d 192.168.2.13 -j DROP
ip netns exec node2 iptables -A INPUT -s 192.168.2.13 -j DROP
ip netns exec node3 iptables -A OUTPUT -d 192.168.2.12 -j DROP
ip netns exec node3 iptables -A INPUT -s 192.168.2.12 -j DROP

echo ""
echo -e "${YELLOW}⏳ Waiting for TDMA timeout detection (10 seconds)...${NC}"

for i in 10 9 8 7 6 5 4 3 2 1; do
    echo -ne "   $i seconds...\r"
    sleep 1
done
echo ""

# ========================================
# Check Rerouting
# ========================================

echo ""
echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}  Check Rerouting                               ${NC}"
echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

echo ""
echo "📝 Node 2 logs (timeout for Node 3):"
grep -E "TIMEOUT.*Node 3|Link.*node 3.*changed" logs/node_2.log | tail -5

echo ""
echo "📝 Node 1 logs (routing updates):"
grep -E "Routing changed|ARP.*→" logs/node_1.log | tail -10

echo ""
echo "📊 Node 1 ARP Table AFTER link break:"
ip netns exec node1 arp -n

NODE4_MAC_AFTER=$(ip netns exec node1 arp -n | grep "192.168.2.14" | awk '{print $3}')

echo ""
echo "🔍 Comparison:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "BEFORE:"
cat /tmp/arp_linear.txt
echo ""
echo "AFTER:"
ip netns exec node1 arp -n
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

if [ -n "$NODE4_MAC" ] && [ -n "$NODE4_MAC_AFTER" ]; then
    if [ "$NODE4_MAC" != "$NODE4_MAC_AFTER" ]; then
        echo ""
        echo -e "${GREEN}✅ REROUTING DETECTED!${NC}"
        echo "   Next-hop MAC changed:"
        echo "   Before: $NODE4_MAC"
        echo "   After:  $NODE4_MAC_AFTER"
    else
        echo ""
        echo -e "${YELLOW}⚠️  No MAC change detected${NC}"
    fi
else
    echo ""
    echo -e "${YELLOW}⚠️  Cannot compare (missing ARP entries)${NC}"
fi

echo ""
echo "🔍 Testing connectivity Node1 → Node4:"
if ip netns exec node1 ping -c 1 -W 2 192.168.2.14 > /dev/null 2>&1; then
    echo -e "${GREEN}✅ Reachable${NC}"
else
    echo -e "${RED}❌ NOT reachable (network partitioned)${NC}"
fi

# ========================================
# Summary
# ========================================

echo ""
echo "╔════════════════════════════════════════════════╗"
echo "║  TEST COMPLETE                                 ║"
echo "╚════════════════════════════════════════════════╝"
echo ""

echo "📋 Summary:"
echo "   1. iptables INPUT+OUTPUT configured BEFORE TDMA"
echo "   2. TDMA detected topology via timeouts"
echo "   3. Link Node2-Node3 broken"
echo "   4. Checked for rerouting"
echo ""

echo "📝 Review full logs:"
echo "   tail -100 logs/node_1.log | grep -E 'TIMEOUT|Routing|ARP'"
echo ""

read -p "Stop TDMA nodes? (y/n) " -n 1 -r
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
echo "Done!"