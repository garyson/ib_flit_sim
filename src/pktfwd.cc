///////////////////////////////////////////////////////////////////////////
//
//         InfiniBand FLIT (Credit) Level OMNet++ Simulation Model
//
// Copyright (c) 2004-2013 Mellanox Technologies, Ltd. All rights reserved.
// Copyright (c) 2014-2016 University of New Hampshire InterOperability Laboratory
// This software is available to you under the terms of the GNU
// General Public License (GPL) Version 2, available from the file
// COPYING in the main directory of this source tree.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
// BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
///////////////////////////////////////////////////////////////////////////
#include "pktfwd.h"
#include <vec_file.h>
#include "obuf.h"
using namespace omnetpp;

Define_Module(Pktfwd);

void Pktfwd::initialize() {

	Switch = getParentModule();
	if (!Switch) {
		throw cRuntimeError("-E- Failed to obtain an parent Switch module");
	}
	numPorts = Switch->par("numSwitchPorts");

	// setup pointer to FDB
	const char *fdbsFile = par("fdbsVecFile");
	int fdbIdx = par("fdbIndex");
	vecFiles *vecMgr = vecFiles::get();
	FDB = vecMgr->getIntVec(fdbsFile, fdbIdx);
	if (FDB == NULL) {
		throw cRuntimeError("-E- Failed to obtain an FDB %s, %d", fdbsFile, fdbIdx);
	} else {
		EV<< "-I- " << getFullPath() << " Obtained FDB of size:"
		<< FDB->size() << endl;
	}
}

// get the output port for the given LID - the actual AR or deterministic routing
int Pktfwd::getPortByLID(unsigned int lid) {
	Enter_Method("getPortByLID LID: %d", lid);
	unsigned int outPort; // the resulting output port
	if (lid >= FDB->size()) {
		throw cRuntimeError("-E- getPortByLID: LID %d is out of available FDB range %d",
				lid, FDB->size() - 1);
	}

	outPort = (*FDB)[lid];
	return(outPort);
}

// report queuing of flits on TQ for DLID (can be negative for arb)
int Pktfwd::repQueuedFlits(unsigned int rq, unsigned int tq, unsigned int dlid, int numFlits) {
	Enter_Method("repQueuedFlits tq:%d flits:%d", tq, numFlits);
	return(0);
}

void Pktfwd::finish()
{
}

Pktfwd::~Pktfwd() {
}
