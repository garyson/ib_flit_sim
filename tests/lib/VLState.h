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
