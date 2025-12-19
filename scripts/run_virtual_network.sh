#!/bin/bash
# scripts/run_virtual_network.sh

if [ "$EUID" -ne 0 ]; then
  echo "❌ Error: Please run as root (sudo)"
  exit
fi

# 1. Compila o projeto
echo "🔨 Compiling..."
make clean && make
if [ $? -ne 0 ]; then
    echo "❌ Compilation failed."
    exit 1
fi

# 2. Garante que a rede virtual existe
# CORREÇÃO: Verifica se 'node1' existe (em vez de ns1)
if [ ! -f "/var/run/netns/node1" ]; then
    echo "🌐 Setting up network namespaces..."
    ./scripts/setup_dev_network.sh
fi

# 3. Prepara logs
mkdir -p logs
rm -f logs/*.log
echo "📝 Logs cleared. Output will be in logs/node_X.log"

# 4. Inicia os nós em background
echo "🚀 Starting 4 nodes..."

# Estratégia 0 = Dijkstra
# Sintaxe: ./tdma_daemon <id> <total> <strategy>
# CORREÇÃO: Usa 'nodeX' em vez de 'nsX' nos comandos abaixo

ip netns exec node1 ./build/tdma_daemon 1 4 0 > logs/node_1.log 2>&1 &
PID1=$!
echo "   [+] Node 1 started (PID $PID1)"

ip netns exec node2 ./build/tdma_daemon 2 4 0 > logs/node_2.log 2>&1 &
PID2=$!
echo "   [+] Node 2 started (PID $PID2)"

ip netns exec node3 ./build/tdma_daemon 3 4 0 > logs/node_3.log 2>&1 &
PID3=$!
echo "   [+] Node 3 started (PID $PID3)"

ip netns exec node4 ./build/tdma_daemon 4 4 0 > logs/node_4.log 2>&1 &
PID4=$!
echo "   [+] Node 4 started (PID $PID4)"

echo ""
echo "✅ Network is RUNNING!"
echo "   - Tail Node 1 logs:  tail -f logs/node_1.log"
echo "   - Tail Node 2 logs:  tail -f logs/node_2.log"
echo ""
echo "Press [ENTER] to stop the network..."
read

# Cleanup quando o user pressionar Enter
echo "🛑 Stopping nodes..."
kill $PID1 $PID2 $PID3 $PID4 2>/dev/null
wait $PID1 $PID2 $PID3 $PID4 2>/dev/null
echo "Done."