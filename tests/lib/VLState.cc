/* VLState.cc */

#include "VLState.h"
#include "ib_m.h"

using namespace omnetpp;

VLState::VLState(unsigned int id, unsigned int expectedFCPCount)
    : id(id), totalExpectedFCPCount(expectedFCPCount)
{
}

VLState::~VLState()
{
    /* No behavior; used simply to correctly destroy the object given a
     * pointer to a subclass */
}

void VLState::sendCreditUpdate(cSimpleModule *source, const char *gateName,
                               simtime_t time)
{
    nextFCCL++;

    std::ostringstream namegen;
    namegen << "rxcred-test-msg-" << creditUpdateCount++;
    IBRxCredMsg *msg = new IBRxCredMsg(namegen.str().c_str(), IB_RXCRED_MSG);
    msg->setVL(id);
    msg->setFCCL(nextFCCL);
    /* sendDelayed transfers ownership of the message to the receiver */
    std::cout << "-I- Send credit update on vl=" << id << " FCCL="
              << nextFCCL << " at t=" << time << "\n";
    source->sendDelayed(msg, time, gateName);
}

void VLState::acceptFCP()
{
    ++expectedFCPCount;
}

void VLState::handleRecvFCP(IBFlowControl *msg)
{
    ++fcpCount;

    if (fcpCount > expectedFCPCount) {
        std::cout << "ERROR: Received unexpected FCP [VL="
                  << id << " sn=" << fcpCount << "]\n";
        return;
    }

    std::cout << "-I- Received FCP on vl=" << id << " FCCL="
              << msg->getFCCL() << "\n";
    if (fcpCount == 1) {
        /* Initialization FCP */
        nextFCCL = msg->getFCCL();
        onFirstFCP(msg);
    } else {
        if (msg->getFCTBS() > FCTBS) {
            /* If just the FCTBS has been incremented, then that is ok---this
             * is just a update to the remote side that we have sent 2 credits. */
            FCTBS = msg->getFCTBS();
        } else if (msg->getFCCL() != nextFCCL) {
            std::cout << "ERROR: [VL=" << id << " sn=" << fcpCount
                      << "] expected FCCL " << nextFCCL
                      << " got " << msg->getFCCL() << '\n';
        }
    }
}

void VLState::finish()
{
    std::cout << ((fcpCount == totalExpectedFCPCount) ? "OK" : "ERROR")
              << ": [vl=" << id << "] Received " << fcpCount
              << " flow control messages expected "
              << totalExpectedFCPCount << "\n";
}

void VLState::onFirstFCP(IBFlowControl *msg)
{
}
