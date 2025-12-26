#!/bin/bash
# scripts/test_link_failure.sh - Testa link failure e mostra mudanças ARP

if [ "$EUID" -ne 0 ]; then
    echo "❌ Please run as root (sudo)"
    exit 1
fi

# Cores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# IPs definidos no setup_dev_network.sh
IP_NODE1="192.168.2.11"
IP_NODE2="192.168.2.12"
IP_NODE3="192.168.2.13"
IP_NODE4="192.168.2.14"

# Função para mostrar ARP table de um node
show_arp_table() {
    local NODE=$1
    echo ""
    echo -e "${BLUE}╔════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║  ARP TABLE - $NODE                              ${NC}"
    echo -e "${BLUE}╚════════════════════════════════════════════════╝${NC}"
    ip netns exec $NODE arp -n | head -1  # Header
    ip netns exec $NODE arp -n | grep -v "Address" | sort
    echo ""
}

# Função para mostrar routing table de um node
show_routing_table() {
    local NODE=$1
    echo ""
    echo -e "${BLUE}╔════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║  ROUTING INFO - $NODE                           ${NC}"
    echo -e "${BLUE}╚════════════════════════════════════════════════╝${NC}"
    ip netns exec $NODE ip route show
    echo ""
}

# Função para mostrar estado de conectividade
test_connectivity() {
    echo ""
    echo -e "${BLUE}╔════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║  CONNECTIVITY TEST                             ${NC}"
    echo -e "${BLUE}╚════════════════════════════════════════════════╝${NC}"
    
    for src in 1 2 3 4; do
        for dst in 1 2 3 4; do
            if [ $src -ne $dst ]; then
                SRC_NODE="node$src"
                DST_IP="192.168.2.$((10+dst))"
                
                echo -n "   $SRC_NODE → node$dst ($DST_IP): "
                
                if ip netns exec $SRC_NODE ping -c 1 -W 1 $DST_IP > /dev/null 2>&1; then
                    echo -e "${GREEN}✅ OK${NC}"
                else
                    echo -e "${RED}❌ FAIL${NC}"
                fi
            fi
        done
    done
    echo ""
}

# Verifica se a rede está criada
if ! ip netns list | grep -q "node1"; then
    echo "❌ Network namespaces not found!"
    echo "Run: make setup_network"
    exit 1
fi

# Verifica se os nodes estão a correr
if ! pgrep -f "tdma_node" > /dev/null; then
    echo "⚠️  Warning: TDMA nodes don't seem to be running"
    echo "Run in another terminal: make run_network"
    echo ""
    read -p "Continue anyway? (y/n) " -n 1 -r
    echo ""
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

echo ""
echo "╔════════════════════════════════════════════════╗"
echo "║  LINK FAILURE SIMULATION TEST                  ║"
echo "╚════════════════════════════════════════════════╝"
echo ""

# ========================================
# FASE 1: Estado Inicial
# ========================================

echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}  PHASE 1: Initial State (All Links UP)        ${NC}"
echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

test_connectivity

echo "📊 ARP Tables BEFORE link failure:"
show_arp_table "node1"
show_arp_table "node2"
show_arp_table "node3"

echo ""
echo "💾 Saving initial state..."
ip netns exec node1 arp -n > /tmp/arp_before_node1.txt
ip netns exec node3 arp -n > /tmp/arp_before_node3.txt

echo ""
read -p "Press [ENTER] to simulate link failure..."

# ========================================
# FASE 2: Quebra de Link
# ========================================

echo ""
echo -e "${RED}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${RED}  PHASE 2: Breaking Link (Node1 <-> Node2)     ${NC}"
echo -e "${RED}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

echo ""
echo "🔥 Cutting link between Node 1 and Node 2..."
echo "   (Blocking traffic in both directions)"

# Bloqueia tráfego bidirecional
ip netns exec node1 iptables -A OUTPUT -d $IP_NODE2 -j DROP 2>/dev/null
ip netns exec node2 iptables -A OUTPUT -d $IP_NODE1 -j DROP 2>/dev/null

echo ""
echo -e "${YELLOW}⏳ Waiting for timeout detection (~5 seconds)...${NC}"
sleep 2

for i in 5 4 3 2 1; do
    echo -ne "   $i...\r"
    sleep 1
done
echo ""

test_connectivity

echo "📊 ARP Tables AFTER link failure (should see rerouting):"
show_arp_table "node1"
show_arp_table "node3"

echo ""
echo "🔍 Comparing ARP changes for Node 1:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "BEFORE:"
cat /tmp/arp_before_node1.txt | grep -v "Address"
echo ""
echo "AFTER:"
ip netns exec node1 arp -n | grep -v "Address"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Detecta mudanças específicas
echo ""
echo "🔎 Analyzing changes..."

# Node 1 perspective: Node 4 should now route via Node 3 (not direct via Node 2)
NODE1_TO_NODE4_MAC_BEFORE=$(grep "$IP_NODE4" /tmp/arp_before_node1.txt | awk '{print $3}')
NODE1_TO_NODE4_MAC_AFTER=$(ip netns exec node1 arp -n | grep "$IP_NODE4" | awk '{print $3}')

if [ -n "$NODE1_TO_NODE4_MAC_BEFORE" ] && [ -n "$NODE1_TO_NODE4_MAC_AFTER" ]; then
    if [ "$NODE1_TO_NODE4_MAC_BEFORE" != "$NODE1_TO_NODE4_MAC_AFTER" ]; then
        echo -e "${GREEN}✅ REROUTING DETECTED!${NC}"
        echo "   Node 1 → Node 4:"
        echo "   Before: MAC $NODE1_TO_NODE4_MAC_BEFORE"
        echo "   After:  MAC $NODE1_TO_NODE4_MAC_AFTER"
        echo ""
        echo "   Explanation: Traffic to Node 4 is now using a different next-hop!"
    else
        echo -e "${YELLOW}⚠️  No MAC change detected (route may still be same)${NC}"
    fi
else
    echo -e "${YELLOW}⚠️  Could not compare MACs (entries missing)${NC}"
fi

echo ""
echo "📝 Check TDMA logs for routing version changes:"
echo "   tail -20 logs/node_1.log | grep -E 'Routing changed|ARP'"

echo ""
read -p "Press [ENTER] to repair the link..."

# ========================================
# FASE 3: Restaurar Link
# ========================================

echo ""
echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}  PHASE 3: Repairing Link                       ${NC}"
echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

echo ""
echo "🚑 Restoring link between Node 1 and Node 2..."

# Remove iptables rules
ip netns exec node1 iptables -D OUTPUT -d $IP_NODE2 -j DROP 2>/dev/null
ip netns exec node2 iptables -D OUTPUT -d $IP_NODE1 -j DROP 2>/dev/null

echo ""
echo -e "${YELLOW}⏳ Waiting for route re-optimization (~5 seconds)...${NC}"
sleep 2

for i in 5 4 3 2 1; do
    echo -ne "   $i...\r"
    sleep 1
done
echo ""

test_connectivity

echo "📊 ARP Tables AFTER repair (should optimize back):"
show_arp_table "node1"
show_arp_table "node3"

echo ""
echo "🔍 Final comparison for Node 1:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "INITIAL STATE:"
cat /tmp/arp_before_node1.txt | grep -v "Address"
echo ""
echo "AFTER FAILURE:"
echo "(already shown above)"
echo ""
echo "AFTER REPAIR:"
ip netns exec node1 arp -n | grep -v "Address"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

NODE1_TO_NODE4_MAC_FINAL=$(ip netns exec node1 arp -n | grep "$IP_NODE4" | awk '{print $3}')

if [ "$NODE1_TO_NODE4_MAC_BEFORE" == "$NODE1_TO_NODE4_MAC_FINAL" ]; then
    echo -e "${GREEN}✅ ROUTE OPTIMIZED BACK!${NC}"
    echo "   Node 1 → Node 4 is using original path again"
else
    echo -e "${YELLOW}⚠️  Route has not reverted (may be using different strategy)${NC}"
fi

# Cleanup temp files
rm -f /tmp/arp_before_*.txt

echo ""
echo "╔════════════════════════════════════════════════╗"
echo "║  TEST COMPLETE                                 ║"
echo "╚════════════════════════════════════════════════╝"
echo ""
echo "📋 Summary:"
echo "   1. Initial state captured"
echo "   2. Link Node1-Node2 broken → rerouting triggered"
echo "   3. Link repaired → route optimized back"
echo ""
echo "💡 Tips:"
echo "   - Check logs: tail -f logs/node_1.log"
echo "   - Manual ARP check: sudo ip netns exec node1 arp -n"
echo "   - Routing table: sudo ip netns exec node1 ip route"
echo ""