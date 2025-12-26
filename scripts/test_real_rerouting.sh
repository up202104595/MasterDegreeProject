#!/bin/bash
# scripts/test_real_rerouting.sh - Força topologia específica

if [ "$EUID" -ne 0 ]; then
    echo "❌ Please run as root"
    exit 1
fi

echo "╔════════════════════════════════════════════════╗"
echo "║  REAL REROUTING TEST                           ║"
echo "╚════════════════════════════════════════════════╝"
echo ""

# Verifica se TDMA está a correr
if ! pgrep -f "tdma_node" > /dev/null; then
    echo "❌ TDMA not running. Start with: make run_network"
    exit 1
fi

echo "📐 Setting up controlled topology:"
echo ""
echo "   Initial: Node1 --- Node2 --- Node3 --- Node4"
echo "                      (linear)"
echo ""

# Limpa iptables
for i in 1 2 3 4; do
    ip netns exec node$i iptables -F 2>/dev/null
done

# ========================================
# FASE 1: Criar Topologia Linear
# ========================================

echo "🔧 Creating LINEAR topology (Node 1 can't reach Node 4 directly)..."

# Node 1 só pode falar com Node 2
ip netns exec node1 iptables -A OUTPUT -d 192.168.2.13 -j DROP
ip netns exec node1 iptables -A OUTPUT -d 192.168.2.14 -j DROP

# Node 2 só pode falar com Node 1 e Node 3
ip netns exec node2 iptables -A OUTPUT -d 192.168.2.14 -j DROP

# Node 3 só pode falar com Node 2 e Node 4
ip netns exec node3 iptables -A OUTPUT -d 192.168.2.11 -j DROP

# Node 4 só pode falar com Node 3
ip netns exec node4 iptables -A OUTPUT -d 192.168.2.11 -j DROP
ip netns exec node4 iptables -A OUTPUT -d 192.168.2.12 -j DROP

echo "✅ Topology set"
echo ""
echo "⏳ Waiting for TDMA to detect topology (10 seconds)..."
sleep 10

echo ""
echo "📊 Testing connectivity in LINEAR topology:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

for src in 1 2 3 4; do
    for dst in 1 2 3 4; do
        if [ $src -ne $dst ]; then
            result=$(ip netns exec node$src ping -c 1 -W 1 192.168.2.$((10+dst)) 2>/dev/null && echo "✅" || echo "❌")
            echo "   node$src → node$dst: $result"
        fi
    done
done

echo ""
echo "💾 Saving ARP state in LINEAR topology..."
ip netns exec node1 arp -n > /tmp/arp_linear_node1.txt

echo ""
echo "📊 Node 1 ARP Table (LINEAR topology):"
ip netns exec node1 arp -n

echo ""
echo "🔍 Expected: Node1 → Node4 should use Node2's MAC as next-hop"
NODE2_MAC=$(ip netns exec node1 arp -n | grep "192.168.2.12" | awk '{print $3}')
NODE4_MAC=$(ip netns exec node1 arp -n | grep "192.168.2.14" | awk '{print $3}')

if [ "$NODE2_MAC" == "$NODE4_MAC" ]; then
    echo "✅ CORRECT: Node4 is routed via Node2 (MAC: $NODE2_MAC)"
else
    echo "⚠️  Node4 MAC ($NODE4_MAC) ≠ Node2 MAC ($NODE2_MAC)"
    echo "   Check logs: tail -30 logs/node_1.log"
fi

echo ""
read -p "Press [ENTER] to break Node2-Node3 link (force reroute)..."

# ========================================
# FASE 2: Quebrar Link Node2-Node3
# ========================================

echo ""
echo "🔥 BREAKING LINK: Node2 <-> Node3"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Bloqueia link Node2-Node3
ip netns exec node2 iptables -A OUTPUT -d 192.168.2.13 -j DROP
ip netns exec node3 iptables -A OUTPUT -d 192.168.2.12 -j DROP

echo "⏳ Waiting for timeout detection (7 seconds)..."
sleep 7

echo ""
echo "📊 Testing connectivity after link break:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

for src in 1 4; do
    for dst in 1 4; do
        if [ $src -ne $dst ]; then
            result=$(ip netns exec node$src ping -c 1 -W 1 192.168.2.$((10+dst)) 2>/dev/null && echo "✅" || echo "❌")
            echo "   node$src → node$dst: $result"
        fi
    done
done

echo ""
echo "📊 Node 1 ARP Table AFTER link break:"
ip netns exec node1 arp -n

echo ""
echo "🔍 Analyzing changes..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

NODE4_MAC_AFTER=$(ip netns exec node1 arp -n | grep "192.168.2.14" | awk '{print $3}')

echo "BEFORE (LINEAR):"
cat /tmp/arp_linear_node1.txt | grep "192.168.2.14"

echo ""
echo "AFTER (LINK BREAK):"
ip netns exec node1 arp -n | grep "192.168.2.14"

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

if [ "$NODE4_MAC" != "$NODE4_MAC_AFTER" ]; then
    echo "✅ REROUTING DETECTED!"
    echo "   Node1 → Node4 changed next-hop MAC:"
    echo "   Before: $NODE4_MAC (via Node2)"
    echo "   After:  $NODE4_MAC_AFTER (alternate route)"
else
    echo "⚠️  No rerouting detected"
    echo ""
    echo "📝 Check TDMA logs:"
    echo "   grep -E 'TIMEOUT|Link.*changed|Routing' logs/node_1.log | tail -20"
fi

echo ""
echo "🧹 Cleanup: Removing all iptables rules..."
for i in 1 2 3 4; do
    ip netns exec node$i iptables -F 2>/dev/null
done

echo "✅ Test complete"