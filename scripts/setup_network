#!/bin/bash
# scripts/setup_dev_network.sh
# Creates virtual network with proper routing

if [ "$EUID" -ne 0 ]; then
    echo "❌ Please run as root (sudo)"
    exit 1
fi

echo "╔════════════════════════════════════════════════╗"
echo "║  Setting Up Virtual Network (4 Nodes)         ║"
echo "╚════════════════════════════════════════════════╝"
echo ""

# ========================================
# Cleanup
# ========================================

echo "🧹 Cleaning up..."

killall -9 tdma_node 2>/dev/null
sleep 1

for i in 1 2 3 4; do
    ip netns del node$i 2>/dev/null
done

ip link del br0 2>/dev/null

for i in 1 2 3 4; do
    ip link del veth$i 2>/dev/null
    ip link del veth${i}_br 2>/dev/null
done

sleep 1

# ========================================
# Create Bridge
# ========================================

echo ""
echo "🌉 Creating bridge..."

ip link add br0 type bridge
ip link set br0 up

echo "   ✅ Bridge br0 created"

# ========================================
# Create Namespaces + Interfaces
# ========================================

echo ""
echo "📦 Creating namespaces..."

for i in 1 2 3 4; do
    NODE_IP="192.168.2.1$i"  # .11, .12, .13, .14
    
    # Create namespace
    ip netns add node$i
    
    # Create veth pair
    ip link add veth$i type veth peer name veth${i}_br
    
    # Move one end to namespace
    ip link set veth$i netns node$i
    
    # Attach other end to bridge
    ip link set veth${i}_br master br0
    ip link set veth${i}_br up
    
    # Configure inside namespace
    ip netns exec node$i ip link set lo up
    ip netns exec node$i ip link set veth$i up
    ip netns exec node$i ip addr add ${NODE_IP}/24 dev veth$i
    
    # CRITICAL: Disable RPF (permite receber pacotes de qualquer fonte)
    ip netns exec node$i sysctl -w net.ipv4.conf.all.rp_filter=0 >/dev/null 2>&1
    ip netns exec node$i sysctl -w net.ipv4.conf.default.rp_filter=0 >/dev/null 2>&1
    ip netns exec node$i sysctl -w net.ipv4.conf.veth$i.rp_filter=0 >/dev/null 2>&1
    
    # Enable IP forwarding
    ip netns exec node$i sysctl -w net.ipv4.ip_forward=1 >/dev/null 2>&1
    
    # CRITICAL: Disable ICMP redirects
    ip netns exec node$i sysctl -w net.ipv4.conf.all.send_redirects=0 >/dev/null 2>&1
    ip netns exec node$i sysctl -w net.ipv4.conf.veth$i.send_redirects=0 >/dev/null 2>&1
    
    echo "   ✅ Node $i: veth$i @ ${NODE_IP}/24"
done

# ========================================
# Verify Network
# ========================================

echo ""
echo "🔍 Verifying network..."

sleep 1

# Test ping between nodes
PING_OK=true

if ip netns exec node1 ping -c 1 -W 2 192.168.2.12 >/dev/null 2>&1; then
    echo "   ✅ Node 1 → Node 2: OK"
else
    echo "   ❌ Node 1 → Node 2: FAIL"
    PING_OK=false
fi

if ip netns exec node1 ping -c 1 -W 2 192.168.2.13 >/dev/null 2>&1; then
    echo "   ✅ Node 1 → Node 3: OK"
else
    echo "   ❌ Node 1 → Node 3: FAIL"
    PING_OK=false
fi

if ip netns exec node1 ping -c 1 -W 2 192.168.2.14 >/dev/null 2>&1; then
    echo "   ✅ Node 1 → Node 4: OK"
else
    echo "   ❌ Node 1 → Node 4: FAIL"
    PING_OK=false
fi

if [ "$PING_OK" = "false" ]; then
    echo ""
    echo "❌ Network connectivity FAILED!"
    echo ""
    echo "Debug info:"
    echo "Node 1 interfaces:"
    ip netns exec node1 ip addr show veth1
    echo ""
    echo "Node 1 routes:"
    ip netns exec node1 ip route
    echo ""
    echo "Bridge status:"
    ip link show br0
    echo ""
    exit 1
fi

# ========================================
# Summary
# ========================================

echo ""
echo "╔════════════════════════════════════════════════╗"
echo "║  NETWORK SETUP COMPLETE                        ║"
echo "╚════════════════════════════════════════════════╝"
echo ""

echo "🌐 Topology:"
echo "   Bridge:  br0 (layer 2 switch)"
echo "   Node 1:  veth1 @ 192.168.2.11/24"
echo "   Node 2:  veth2 @ 192.168.2.12/24"
echo "   Node 3:  veth3 @ 192.168.2.13/24"
echo "   Node 4:  veth4 @ 192.168.2.14/24"
echo ""

echo "📋 Example routing table (Node 1):"
ip netns exec node1 ip route | head -3

echo ""
echo "✅ Network is ready!"
echo ""
echo "Next steps:"
echo "   Run network: sudo ./scripts/run_virtual_network.sh"
echo "   Or manual:   sudo ip netns exec node1 ./build/tdma_node 1 4 2"
echo ""