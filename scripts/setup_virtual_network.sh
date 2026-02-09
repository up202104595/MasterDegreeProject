#!/bin/bash
# setup_virtual_network.sh - Create virtual network with network namespaces

if [ "$EUID" -ne 0 ]; then
    echo "❌ Must run as root (use sudo)"
    exit 1
fi

if [ $# -lt 1 ]; then
    echo "Usage: $0 <num_nodes>"
    echo "Example: $0 4"
    exit 1
fi

NUM_NODES=$1

echo "╔════════════════════════════════════════════════╗"
echo "║  Setting up virtual network                    ║"
echo "║  Nodes: $NUM_NODES                                     ║"
echo "╚════════════════════════════════════════════════╝"
echo ""

# Cleanup first
./scripts/cleanup_virtual_network.sh 2>/dev/null

# Create namespaces and veth pairs
for i in $(seq 1 $NUM_NODES); do
    echo "🔧 Creating node$i..."
    
    # Create namespace
    ip netns add node$i
    
    # Create veth pair
    ip link add veth${i} type veth peer name veth${i}-br
    
    # Move one end to namespace
    ip link set veth${i} netns node$i
    
    # Configure namespace interface
    ip netns exec node$i ip addr add 192.168.2.${i}/24 dev veth${i}
    ip netns exec node$i ip link set veth${i} up
    ip netns exec node$i ip link set lo up
    
    # Configure bridge side
    ip link set veth${i}-br up
    
    # Add to bridge (create if doesn't exist)
    if ! ip link show br-tdma >/dev/null 2>&1; then
        ip link add name br-tdma type bridge
        ip link set br-tdma up
    fi
    
    ip link set veth${i}-br master br-tdma
    
    echo "   ✅ node$i created (192.168.2.${i})"
done

echo ""
echo "✅ Network setup complete!"
echo ""
echo "Test connectivity:"
echo "  sudo ip netns exec node1 ping -c 2 192.168.2.2"