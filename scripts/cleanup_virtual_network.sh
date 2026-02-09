#!/bin/bash
# cleanup_virtual_network.sh - Remove all virtual network namespaces

if [ "$EUID" -ne 0 ]; then
    echo "❌ Must run as root (use sudo)"
    exit 1
fi

echo "🧹 Cleaning up virtual network..."

# Kill any running tdma_node processes
killall -9 tdma_node 2>/dev/null

# Remove all node namespaces
for ns in $(ip netns list | grep node | awk '{print $1}'); do
    echo "   Removing $ns..."
    ip netns del $ns 2>/dev/null
done

# Remove bridge
ip link del br-tdma 2>/dev/null

# Remove any remaining veth interfaces
for iface in $(ip link show | grep veth | awk -F: '{print $2}' | tr -d ' '); do
    echo "   Removing $iface..."
    ip link del $iface 2>/dev/null
done

echo "✅ Cleanup complete!"