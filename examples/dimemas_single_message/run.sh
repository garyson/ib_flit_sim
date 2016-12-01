#!/bin/bash
# examples/dimemas_single_message/run.sh
#
# InfiniBand FLIT (Credit) Level OMNet++ Simulation Model
#
# Copyright (c) 2015-2016 University of New Hampshire InterOperability Laboratory
#
# This software is available to you under the terms of the GNU
# General Public License (GPL) Version 2, available from the file
# COPYING in the main directory of this source tree.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
# BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
# ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

module load toolchain/system omnetpp dimemas paraver

MODELDIR=$(dirname $0)/../..
TRACE_BASENAME=trace

prv2dim ${TRACE_BASENAME}.prv ${TRACE_BASENAME}.dim
if [[ -z ${TMUX} ]]; then
  echo "*** Starting new tmux session"
  NEWW="new-session -s omnet-run"
else
  echo "+++ Using existing tmux session"
  NEWW=new-window
fi

echo "+++ $(date) ${dir} Running co-simulation in new tmux window"
tmux ${NEWW} -P -n omnet-run -d "
	trap 'tmux wait-for -S omnet-run-lock' EXIT;
	module load omnetpp;
        opp_run -l ${MODELDIR}/src/libib_flit_sim.so -f ${MODELDIR}/networks/two_fdr_nodes.ini -n ..:${MODELDIR}/networks:${MODELDIR}/src -u Cmdenv --result-dir=results 2>&1 | tee omnetpp.log
        "
sleep 5
tmux split-window -v -t omnet-run "module load dimemas;
        Dimemas -S 32M --dim ${TRACE_BASENAME}.dim -venus -p predicted.prv dimemas.cfg 2>&1 | tee dimemas.log"
if [[ ${NEWW} == new-window ]]; then
  tmux select-window -t omnet-run
  tmux wait-for omnet-run-lock
else
  tmux attach-session -t omnet-run
fi

echo "+++ $(date) ${dir} co-simulation run is complete"
echo "+++ Dimemas output is in predicted.prv"
echo "+++ OMNet++ output is in results directory"
echo
