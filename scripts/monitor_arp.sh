#!/bin/bash
# scripts/monitor_arp.sh - Monitoriza ARP tables em tempo real

if [ "$EUID" -ne 0 ]; then
    echo "❌ Please run as root (sudo)"
    exit 1
fi

NODE=${1:-node1}  # Default: node1

if ! ip netns list | grep -q "$NODE"; then
    echo "❌ Namespace $NODE not found"
    exit 1
fi

echo "╔════════════════════════════════════════════════╗"
echo "║  ARP Table Monitor - $NODE                      "
echo "╚════════════════════════════════════════════════╝"
echo ""
echo "Press CTRL+C to stop"
echo ""

while true; do
    clear
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "  ARP Table - $NODE - $(date +%H:%M:%S)"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    ip netns exec $NODE arp -n
    echo ""
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "  Routing Table"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    ip netns exec $NODE ip route show
    echo ""
    sleep 1
done