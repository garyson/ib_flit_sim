// tests/lib/VLState.h
//
// InfiniBand FLIT (Credit) Level OMNet++ Simulation Model
//
// Copyright (c) 2015-2016 University of New Hampshire InterOperability Laboratory
//
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

/* VLState.h */

#ifndef VLSTATE_H
#define VLSTATE_H

#include <omnetpp.h>
#include <string>

class IBFlowControl;

///
/// This contains the virtual lane state behavior used by some tests
///

class VLState {
public:
    VLState(unsigned int id, unsigned int expectedFCPCount = 1);
    virtual ~VLState();
    void sendCreditUpdate(omnetpp::cSimpleModule *source, const char *gateName,
                          omnetpp::simtime_t time);
    void acceptFCP();
    void handleRecvFCP(IBFlowControl *msg);
    void finish();

protected:
    virtual void onFirstFCP(IBFlowControl *msg);
        // Override in subclasses to define behavior when the first FCP is
        // received on a virtual lane

    unsigned int id;
    unsigned int nextFCCL = 0;
    unsigned int FCTBS = 0;
    unsigned int creditUpdateCount = 0;
    unsigned int fcpCount = 0;
    unsigned int expectedFCPCount = 1;
    unsigned int totalExpectedFCPCount;
};

#endif
