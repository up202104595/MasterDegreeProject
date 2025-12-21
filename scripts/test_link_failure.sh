#!/bin/bash
# scripts/test_link_failure.sh

if [ "$EUID" -ne 0 ]; then
  echo "Please run as root"
  exit
fi

# IPs definidos no setup_dev_network.sh
IP_NODE1="192.168.2.11"
IP_NODE2="192.168.2.12"

echo "==========================================="
echo "🔥  SIMULATING LINK FAILURE (Node 1 <-> 2)"
echo "==========================================="

# Bloqueia tráfego de saída no NS1 destinado ao NS2
echo "1. Cutting link from Node 1 to Node 2..."
ip netns exec node1 iptables -A OUTPUT -d $IP_NODE2 -j DROP

# Bloqueia tráfego de saída no NS2 destinado ao NS1 (para garantir falha total)
echo "2. Cutting link from Node 2 to Node 1..."
ip netns exec node2 iptables -A OUTPUT -d $IP_NODE1 -j DROP

echo ""
echo "✅ LINK BROKEN! Wait ~4 seconds for timeout detection..."
echo "   (Monitor logs/node_1.log to see the rerouting)"
echo ""
echo "Waiting 15 seconds before repairing..."
sleep 15

echo "==========================================="
echo "🚑  REPAIRING LINK"
echo "==========================================="

# Remove as regras (restaura o link)
ip netns exec node1 iptables -D OUTPUT -d $IP_NODE2 -j DROP
ip netns exec node2 iptables -D OUTPUT -d $IP_NODE1 -j DROP

echo "✅ Link restored. Routes should optimize back to direct path."
