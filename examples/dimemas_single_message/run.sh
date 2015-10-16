#!/bin/sh

module load toolchain/system omnetpp dimemas paraver

tmux new-session -s omnet-run -d "../../out/gcc-debug/src/ib_flit_sim -n ..:../../src -c One -u Cmdenv 2>&1 | tee omnetpp.log"
tmux split-window -v -t omnet-run "Dimemas -S 32K --dim trace.dim -venus -p predicted.prv dimemas.cfg 2>&1 | tee dimemas.log"
tmux attach-session -t omnet-run

echo "Run is complete"
echo "Dimemas output is in predicted.prv"
echo "OMNet++ output is in results directory"
echo
