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
//
// IB App Message Generator:
//
// Internal Messages:
// None
//
// External Messages:
// done/any - IN: the last provided appMsg was consumed - gen a new one
// IBAppMsg - OUT: send the message to the gen
//
// For a full description read the app.h
//

#include "ib_m.h"
#include "app.h"
#include "vlarb.h"
#include <vec_file.h>
#include <cmath>
using namespace std;
using namespace omnetpp;

Define_Module(IBApp);

void IBApp::parseIntListParam(const char *parName, std::vector<int> &out)
{
  int cnt = 0;
  const char *str = par(parName);
  char *tmpBuf = new char[strlen(str)+1];
  strcpy(tmpBuf, str);
  char *entStr = strtok(tmpBuf, " ,");
  if (out.size()) out.clear();
  while (entStr) {
    cnt++;
    out.push_back(atoi(entStr));
    entStr = strtok(NULL, " ,");
  }
  delete[] tmpBuf;
}

// main init of the module
void IBApp::initialize(){
  // init destination sequence related params
  dstSeqIdx = 0;
  dstSeqDone = 0;
  msgIdx = 0;

  //  destination mode
  const char *dstModePar = par("dstMode");
  if (!strcmp(dstModePar, "param")) {
    msgDstMode = DST_PARAM;
  } else if (!strcmp(dstModePar, "seq_once")) {
    msgDstMode = DST_SEQ_ONCE;
  } else if (!strcmp(dstModePar, "seq_loop")) {
    msgDstMode = DST_SEQ_LOOP;
  } else if (!strcmp(dstModePar, "seq_rand")) {
    msgDstMode = DST_SEQ_RAND;
  } else if (!strcmp(dstModePar, "queue")) {
    msgDstMode = DST_QUEUE;
  } else {
    error("unknown dstMode: %s", dstModePar);
  }

  // destination related parameters
  if (msgDstMode != DST_PARAM && msgDstMode != DST_QUEUE) {
    const char *dstSeqVecFile = par("dstSeqVecFile");
    const int   dstSeqVecIdx  = par("dstSeqVecIdx");
    vecFiles   *vecMgr = vecFiles::get();
    dstSeq = vecMgr->getIntVec(dstSeqVecFile, dstSeqVecIdx);
    if (dstSeq == NULL) {
            throw cRuntimeError("fail to obtain dstSeq vector: %s/%d",
                               dstSeqVecFile, dstSeqVecIdx);
    }
    EV << "-I- Defined DST sequence of " << dstSeq->size() << " LIDs" << endl;
  }

  // Message Length Modes
  const char *msgLenModePar = par("msgLenMode");
  if (!strcmp(msgLenModePar,"param")) {
    msgLenMode = MSG_LEN_PARAM;
  } else if (!strcmp(msgLenModePar,"set")) {
    msgLenMode = MSG_LEN_SET;
  } else {
    throw cRuntimeError("unknown msgLenMode: %s", msgLenMode);
  }

  // need to init the set...
  if (msgLenMode == MSG_LEN_SET) {
    parseIntListParam("msgLenSet", msgLenSet);
    vector<int> msgLenProbVec;
    parseIntListParam("msgLenProb", msgLenProbVec);

    if (msgLenSet.size() != msgLenProbVec.size()) {
      error("provided msgLenSet size: %d != msgLenProb size: %d",
            msgLenSet.size(), msgLenProbVec.size());
    }

    // convert the given probabilities into a histogram
    // with Prob[idx] where idx is the index of the length in the vector
    msgLenProb.setNumCells(msgLenSet.size());
    msgLenProb.setRange(0,msgLenSet.size()-1);
    // HACK: there must be a faster way to do this!
    for (unsigned int i = 0; i < msgLenProbVec.size(); i++)
      for (int p = 0; p < msgLenProbVec[i]; p++)
        msgLenProb.collect(i);

    EV << "-I- Defined Length Set of " << msgLenSet.size() << " size" << endl;
  }

  seqIdxVec.setName("Dst-Sequence-Index");

  // if we are in param mode we may be getting a 0 as DST and thus keep quite
  double delay_ns = par("initialDelay");
  if (msgDstMode == DST_QUEUE) {
      delay_ns = 0;
  }
  if (msgDstMode == DST_PARAM) {
    int dstLid = par("dstLid");
    if (dstLid)
      scheduleAt(simTime() + 1e-9 * delay_ns, new cMessage);
  } else {
    // Emulate a "done"
    scheduleAt(simTime() + 1e-9 * delay_ns, new cMessage);
  }

}

// get random msg len by the histogram
unsigned int IBApp::getMsgLenByDistribution()
{
 double r = msgLenProb.draw();
 return int(r);
}

// Initialize the parameters for a new message by sampling the
// relevant parameters and  allocate and init a new message
IBAppMsg *IBApp::getNewMsg()
{
  static const unsigned int controlMessageSize = 64;

  unsigned int msgMtuLen_B; // MTU of packet. same for entire message.
  unsigned int msgLen_P;    // the message length in packets
  unsigned int msgLen_B;    // the length of a message in bytes
  unsigned int msgSQ;       // the SQ to be used
  unsigned int msgDstLid;   // destination lid
  unsigned int ourMsgId;    // Dimemas message unique ID

  msgMtuLen_B = par("msgMtuLen");
  msgSQ = par("msgSQ");

  // obtain the message length
  switch (msgLenMode) {
  case MSG_LEN_PARAM:
    msgLen_B = par("msgLength");
    break;
  case MSG_LEN_SET:
    msgLen_B = getMsgLenByDistribution();
    break;
  default:
    error("unsupported msgLenMode: %d", msgLenMode);
    break;
  }

  msgLen_P = msgLen_B / msgMtuLen_B;

  // obtain the message destination
  switch (msgDstMode) {
  case DST_PARAM:
    msgDstLid = par("dstLid");
    ourMsgId = this->msgIdx;
    break;
  case DST_SEQ_ONCE:
    msgDstLid = (*dstSeq)[dstSeqIdx++];
    if (dstSeqIdx == dstSeq->size()) {
            dstSeqDone = 1;
    }
       seqIdxVec.record(dstSeqIdx);
       ourMsgId = this->msgIdx;
    break;
  case DST_SEQ_LOOP:
    msgDstLid = (*dstSeq)[dstSeqIdx++];
    if (dstSeqIdx == dstSeq->size()) {
            dstSeqIdx = 0;
    }
       seqIdxVec.record(dstSeqIdx);
       ourMsgId = this->msgIdx;
    break;
  case DST_SEQ_RAND:
    dstSeqIdx = intuniform(0,dstSeq->size()-1);
    msgDstLid = (*dstSeq)[dstSeqIdx];
    ourMsgId = this->msgIdx;
    break;
  case DST_QUEUE:
    if (msgQueue.empty()) {
      return nullptr;
    } else {
      auto in_msg = msgQueue.front();
      msgQueue.pop();
      msgDstLid = in_msg->getDstLid();
      switch (in_msg->getKind()) {
      case IB_DIM_SEND_MSG:
          msgLen_B = in_msg->getLenBytes() + 64;
          break;
      default:
          msgLen_B = controlMessageSize;
      }
      ourMsgId = in_msg->getMsgId();
    }
    break;
  default:
    error("unsupported msgDstMode: %d", msgDstMode);
    break;
  }

  msgLen_P = std::ceil(msgLen_B / (float)msgMtuLen_B);
  if (msgLen_P == 0) {
	  /* 0 length messages still have 1 packet with headers */
	  msgLen_B = 1;
	  msgLen_P = 1;
  }

  IBAppMsg *p_msg;
  char name[128];
  sprintf(name, "app-%s-%u", getFullPath().c_str(), ourMsgId);
  p_msg = new IBAppMsg(name, IB_APP_MSG);
  p_msg->setAppIdx( getIndex() );
  p_msg->setMsgIdx(ourMsgId);
  p_msg->setDstLid(msgDstLid);
  p_msg->setSQ(msgSQ);
  p_msg->setLenBytes(msgLen_B);
  p_msg->setLenPkts(msgLen_P);
  p_msg->setMtuBytes(msgMtuLen_B);
  msgIdx++;
  return p_msg;
}

void IBApp::handleMessage(cMessage *p_msg){
  auto gate = p_msg->getArrivalGateId();
  if (p_msg->isSelfMessage() || gate == this->findGate("out$i")) {
    if (!dstSeqDone) {
      // generate a new messaeg and send after hiccup
      IBAppMsg *p_new = getNewMsg();

      if (!p_new) {
          /* No messages to send -- we're done for now */
          EV << "-I- " << getFullPath()
             << "Ready to send but queue is empty\n";
          idle = true;
          delete p_msg;
          return;
      }

      double delay_ns = par("msg2msgGap");
      sendDelayed(p_new, delay_ns*1e-9, "out$o");

      EV << "-I- " << getFullPath()
         << " sending new app message " << p_new->getName()
         << endl;
    }
    delete p_msg;
  } else if (gate == this->findGate("in")) {
    if (msgDstMode != DST_QUEUE) {
        std::cerr << "Wrong mode to receive anything from in gate\n";
        return;
    }
    switch (p_msg->getKind()) {
    case IB_DIM_SEND_MSG:
    case IB_DIM_RREQ_MSG:
    case IB_DIM_RTR_MSG:
        break;
    default:
        std::cerr << "Got wrong message \"" << p_msg->getName()
                  << "\" (kind=" << p_msg->getKind() << ") on in gate\n";
        return;
    }
    msgQueue.push(dynamic_cast<DimReqMsg *>(p_msg));
    /* TODO: Figure out if we are allowed to send a message and do so if
     * we can */
    if (idle) {
      EV << "-I- " << getFullPath()
         << " We were idle; sending self-message to dequeue this at "
         << simTime() << "\n";
      idle = false;
      scheduleAt(simTime(), new cMessage);
    }
  }
}

void IBApp::finish()
{
}
