#!/bin/bash
# setup_dev_network.sh

echo "=== Setting up virtual network for TDMA development ==="

# Verifica se tens permissões de root
if [ "$EUID" -ne 0 ]; then
    echo "Please run as root: sudo $0"
    exit 1
fi

# Limpa setup anterior (apaga nós e bridges antigos)
# O 2>/dev/null esconde mensagens de erro se não houver nada para apagar
ip netns list | grep node | xargs -I {} ip netns del {} 2>/dev/null
ip link show | grep veth | awk '{print $2}' | cut -d@ -f1 | xargs -I {} ip link del {} 2>/dev/null
ip link del br0 2>/dev/null

# Número de nós
NUM_NODES=4

# Cria bridge (a ponte que liga tudo)
ip link add br0 type bridge
ip link set br0 up

# Loop para criar os nós
for i in $(seq 1 $NUM_NODES); do
    NODE="node$i"
    IP="192.168.2.$((10+i))"
    
    echo "Creating $NODE with IP $IP"
    
    # Cria namespace (o PC virtual)
    ip netns add $NODE
    
    # Cria o cabo virtual (veth pair)
    ip link add veth${i} type veth peer name veth${i}_br
    
    # Liga uma ponta do cabo ao namespace
    ip link set veth${i} netns $NODE
    
    # Configura a interface dentro do namespace
    ip netns exec $NODE ip link set lo up
    ip netns exec $NODE ip addr add $IP/24 dev veth${i}
    ip netns exec $NODE ip link set veth${i} up
    
    # Liga a outra ponta do cabo à bridge
    ip link set veth${i}_br master br0
    ip link set veth${i}_br up
done

# Configura o IP da bridge (gateway)
ip addr add 192.168.2.1/24 dev br0

echo ""
echo "=== Network setup complete ==="
echo "Nodes created: node1 to node$NUM_NODES"