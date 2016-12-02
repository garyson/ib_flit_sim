///////////////////////////////////////////////////////////////////////////
//
//         InfiniBand FLIT (Credit) Level OMNet++ Simulation Model
//
// Copyright (c) 2004-2013 Mellanox Technologies, Ltd. All rights reserved.
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
//
// IB Device Port Output Buffer
// ============================
// Ports
// * in
// * out
// * rxCred - where updates from VLArb with incoming FlowControl come
//
// Parameters
// * CreditMinRate - the time between credit updates
// * OutRate - the rate by which flits leave the buffer
//
// Internal Events
// * MinTime - cause a new credit update packet to be inserted into
//   the Q if required
// * Pop - packet with one FLIT is sent through OUT if we have what
//   to send
//
// External Events
// * Push - new credit is provided in
// * RxCred - information regarding the IBUF credits
//
// Function
// * FCTBS is a local parameter incremented on every data packet sent
// * On a push the "credit" provided is put into Q. If there was no
//   pending Pop the data is send immediatly out.
// * When RxCred (carry ABR and Free credits per VL) is received it is
//   stored locally.
// * Each MinTime a "credit update" packet is placed in the Q if
//   needed by comparing to previous FCCL and FCTBS update
// * On "Pop" send a "Push" with one credit through OUT
//

#ifndef __OBUF_H
#define __OBUF_H

#include <omnetpp.h>
#include <vector>
#include "ib_m.h"

/** The previous packet/flit that was popped from the queue and
 * sent. */
enum class IBPopType {
    NONE,
    DATA_FLIT,
    DATA_LAST_FLIT,
    FC_PACKET,
};

//
// Output Buffer for sending IB FLITs and VL credit updates
//
class IBOutBuf : public cSimpleModule
{
 private:
  cMessage *p_popMsg;

  // parameters:
  int    qSize;          // Max number of FLITs the Q can handle
  int    maxVL;          // Maximum VL supported by this port

  // data strcture
  int curFlowCtrVL;    // The VL to sent FC on. If == 8 loop back to 0
  bool Enabled;        // Is this port enabled or is it part of a 8x/12x
  cQueue queue;             // holds the outstanding data
  int numDataCreditsQueued; // needed to make sure we do not overflow the qSize
  unsigned int pendingFreeCount; // number of credits to hand back to VLArb
  IBPopType prevPop;        // last pop type (know when "free" msg)
  simtime_t prevFCTime;     // track the last time the VL0 flow control sent
  std::vector<long> prevSentFCCL;  // Sent FCCL per VL
  std::vector<long> prevSentFCTBS; // Sent FCTBS per VL
  std::vector<long> FCTBS; // num data packet flits sent total in this VL
  std::vector<long> FCCL;  // Pending value to be sent on next Credits update

  // Methods
  void sendOutMessage(IBWireMsg *p_msg);
  void qMessage(IBDataMsg *p_msg);
  int  sendFlowControl();
  void handlePop();
  void handleRxCred(IBRxCredMsg *p_msg);
  virtual void initialize();
  virtual void handleMessage(cMessage *msg);
  virtual void finish();
protected:
  virtual ~IBOutBuf();

  // statistics
  cLongHistogram   qDepthHist;      // number of flits in the out queue
  cDoubleHistogram packetStoreHist; // number of packets in the queue
  cDoubleHistogram flowControlDelay;// track the time between flow controls
  cOutVector       qDepth;          // track the Q usage over time
  simtime_t packetHeadTimeStamp; // track time stamp of the current pop packet

  simtime_t firstPktSendTime; // the first send time
  unsigned int totalBytesSent; // total number of bytes sent
  cStdDev flitsSources; // track flit source for Fair Share

public:
   // used by the VLA to validate the last arbitration
   int  getNumFreeCredits() {
      return(qSize - queue.length());
   };

   // used by VLA to know how many data packets were already sent
   int getFCTBS(int vl) {
      return(FCTBS[vl]);
   };

};

#endif
