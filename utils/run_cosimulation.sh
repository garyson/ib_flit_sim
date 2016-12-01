#!/bin/bash
# utils/run_cosimulation.sh
#
# InfiniBand FLIT (Credit) Level OMNet++ Simulation Model
#
# Copyright (c) 2016 University of New Hampshire InterOperability Laboratory
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

MODELDIR=/home/pmacarth/src/omnetpp-workspace/ib_model
TRACE_BASENAME=adjusted
NETWORK=extraflit

echo "Current directory $(pwd)"
${MODELDIR}/utils/make_topology.py ${NETWORK}.config.yml
if [[ ! -f ${TRACE_BASENAME}.dim || ${TRACE_BASENAME}.prv -nt ${TRACE_BASENAME}.dim ]]; then
	prv2dim ${TRACE_BASENAME}.prv ${TRACE_BASENAME}.dim
fi
if [[ -z ${TMUX} ]]; then
    echo "*** Starting new tmux session"
    NEWW="new-session -s omnet-run"
else
    echo "+++ Using existing tmux session"
    NEWW=new-window
fi
echo "+++ $(date) Running co-simulation in new tmux window"
tmux ${NEWW} -P -n omnet-run -d "module load omnetpp; ${MODELDIR}/src/ib_flit_sim -f ${NETWORK}.ini -n .:${MODELDIR}/src -u Cmdenv -c One --result-dir=${PWD}/results 2>&1 | tee omnetpp.log; echo ${PIPESTATUS[0]} >>omnetpp.log; tmux wait-for -S omnet-run-lock"
sleep 5
tmux split-window -v -t omnet-run "module load dimemas; Dimemas -S 13K --dim ${TRACE_BASENAME}.dim -venus -p predicted-${NETWORK}.prv ${PWD}/${NETWORK}.dimemas.cfg 2>&1 | tee dimemas.log"
if [[ ${NEWW} == new-window ]]; then
    tmux select-window -t omnet-run
    tmux wait-for omnet-run-lock
else
    tmux attach-session -t omnet-run
fi
prv_get_durations.sh predicted-${NETWORK}.prv
rm -r ${PWD}/results-${NETWORK}
mv ${PWD}/results ${PWD}/results-${NETWORK}

echo "+++ $(date) co-simulation run is complete"
echo "+++ Dimemas output is in ${PWD}/predicted-${NETWORK}.prv"
echo "+++ OMNet++ output is in ${PWD}/results-${NETWORK} directory"
echo
