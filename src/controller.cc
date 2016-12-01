///////////////////////////////////////////////////////////////////////////
//
//         InfiniBand FLIT (Credit) Level OMNet++ Simulation Model
//
// Copyright (c) 2004-2013 Mellanox Technologies, Ltd. All rights reserved.
// Copyright (c) 2015-2016 University of New Hampshire InterOperability Laboratory
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
#include "controller.h"
#include "vlarb.h"
#include <cctype>
#include <climits>
#include <cmath>
#include <fstream>
using namespace std;
using namespace omnetpp;

#if defined(__GNUC__)
#define UNUSED __attribute__((__unused__))
#else
#define UNUSED
#endif

Define_Module(Controller);

namespace {
using DimemasHandler = bool (Controller::*)(std::string);
}

// main init of the module
void Controller::initialize(){
  timescale = par("dimemasTimescale");
  stopMessage.setSchedulingPriority(SHRT_MAX);

  const char *socketListenHost = par("socketListenHost");
  const char *socketListenPort = par("socketListenPort");

  try {
  // start listening socket
  ListeningSocket listener(socketListenHost, socketListenPort);
  listener.listen();
  std::cerr << "-I- " << getFullPath()
     << " Listening for Dimemas VenusClient connection on "
     << listener.getName() << '\n';

  sock = std::unique_ptr<Socket>(new Socket(listener.accept()));
  EV << "-I- " << getFullPath()
     << " Accepted connection from " << sock->getRemoteName() << '\n';
  } catch (SocketException &ex) {
      std::cerr << ex.message() << '\n';
      throw;
  }

  this->msgIdx = 0;
  this->waitOnDimemas();

  eagerCount = 0;
  rendezvousCount = 0;
}

void Controller::waitOnDimemas()
{
    static std::map<std::string, DimemasHandler> table = {
        {"SEND", &Controller::handleDimemasSend},
        {"STOP", &Controller::handleDimemasStop},
        {"END", &Controller::handleDimemasEnd},
        {"FINISH", &Controller::handleDimemasFinish},
        {"PROTO OK_TO_SEND", &Controller::handleDimemasRReq},
        {"PROTO READY_TO_RECV", &Controller::handleDimemasRTR},
    };

    while (true) {
        std::string line = sock->recvLine();
        if (line.empty()) {
            return;
        }
        EV << "-I- " << getFullPath() << " Received from Dimemas: " << line;
        bool matched = false;
        for (const auto &entry : table) {
            size_t length = entry.first.size();
            if (line.compare(0, length, entry.first) == 0) {
                matched = true;
                if (!((this->*(entry.second))(line.substr(length)))) {
                    return;
                }
                break;
            }
        }
        if (!matched) {
            EV << "-Ii " << getFullPath()
               << "Got unexpected message: \"" << line << "\"\n";
        }
    }
}

bool Controller::handleDimemasFinish(std::string args)
{
    /* This will throw an exception so this method never actually
     * returns. */
    this->endSimulation();
    return false;
}

bool Controller::handleDimemasEnd(std::string args)
{
    return false;
}

bool Controller::handleDimemasStop(std::string args)
{
    double timestamp = std::stod(args);
    simtime_t delay{timestamp * timescale};
    simtime_t cur = simTime();
    if (delay < cur) {
        delay = cur;
    }
    scheduleAt(delay, &this->stopMessage);
    return true;
}

/** Handle a PROTO OK_TO_SEND message received from Dimemas. */
bool Controller::handleDimemasRReq(std::string args)
{
    auto p_msg = makeMessage(IB_DIM_RREQ_MSG, args);
    sendMessage(p_msg);

    return true;
}

/** Handle a PROTO READY_TO_RECV message received from Dimemas. */
bool Controller::handleDimemasRTR(std::string args)
{
    // generate a new message
    auto p_msg = makeMessage(IB_DIM_RTR_MSG, args);

    auto iter = rreqCount.find(make_pair(p_msg->getSrcNode(),
            p_msg->getDstNode()));
    if (iter != rreqCount.end() && iter->second > 0) {
        sendMessage(p_msg);
    } else {
        enqueueRTR(p_msg);
    }

    return true;
}

/** Handle a SEND message from Dimemas. */
bool Controller::handleDimemasSend(std::string args)
{
    // generate a new message
    ++eagerCount;
    auto p_msg = makeMessage(IB_DIM_SEND_MSG, args);
    sendMessage(p_msg, true);

    return true;
}

/** Place a new message from Dimemas at the end of the appropriate receive
 * queue. */
void Controller::enqueueRTR(DimReqMsg *req)
{
    auto key = make_pair(req->getDstNode(), req->getSrcNode());
    auto &queue = recvQueue[key];
    queue.push(req);
}

/** Sends a message to the application object indicated by the source rank. */
void Controller::sendMessage(DimReqMsg *p_msg, bool doDelay)
{
    double delay = 0;
    if (doDelay) {
        delay = (p_msg->getLenBytes() > par("maxInlineData").longValue())
            ? par("perMsgDelay") : par("inlineMsgDelay");
    }
    simtime_t send_time = p_msg->getInject_time() + delay * 1e-9;
    simtime_t cur = simTime();
    if (send_time >= cur) {
        sendDelayed(p_msg, send_time - cur, "out", p_msg->getSrcNode());
    } else {
        /* Due to rounding error, the delay for this message would be negative.
         *   Send immediately to avoid OMNet++ errors. */
        send(p_msg, "out", p_msg->getSrcNode());
    }

    EV << "-I- " << getFullPath()
       << " sending new controller message " << p_msg->getName()
       << endl;
}

int Controller::getLid(unsigned int node)
{
    cModule *end = gate("out", node)->getPathEndGate()->getOwnerModule()->getParentModule();
    return end->par("srcLid");
}

/** Creates a message object given the parameters. */
DimReqMsg *Controller::makeMessage(IB_MSGS msgType, std::string args)
{
  DimReqMsg *p_msg = new DimReqMsg(nullptr, msgType);
  char name[128];
  size_t index;

  double timestamp = std::stod(args, &index);
  args = args.substr(index);

  if (msgType != IB_DIM_RTR_MSG) {
    p_msg->setSrcNode(std::stoi(args, &index));
    args = args.substr(index);

    p_msg->setDstNode(std::stoi(args, &index));
    args = args.substr(index);
  } else {
    p_msg->setDstNode(std::stoi(args, &index));
    args = args.substr(index);

    p_msg->setSrcNode(std::stoi(args, &index));
    args = args.substr(index);
  }

  p_msg->setMpiTag(std::stoi(args, &index));
  args = args.substr(index);

  p_msg->setLenBytes(std::stoi(args, &index));
  args = args.substr(index);

  if (msgType != IB_DIM_RREQ_MSG) {
    p_msg->setEvent_ptr(std::stoull(args, &index, 0));
    args = args.substr(index);

    p_msg->setEvent_out_ptr(std::stoull(args, &index, 0));
    args = args.substr(index);

    int dummy = std::stoi(args, &index, 0);
    int node = (msgType == IB_DIM_RTR_MSG) ? p_msg->getDstNode() : p_msg->getSrcNode();
    if (dummy != node) {
      throw cRuntimeError("Got two src nodes: %d, %d", node, dummy);
    }
    args = args.substr(index);

    dummy = std::stoi(args, &index, 0);
    node = (msgType == IB_DIM_RTR_MSG) ? p_msg->getSrcNode() : p_msg->getDstNode();
    if (dummy != node) {
      throw cRuntimeError("Got two dst nodes: %d, %d", node, dummy);
    }
    args = args.substr(index);
  }

  p_msg->setSrcApp(std::stoi(args, &index));
  args = args.substr(index);

  p_msg->setDstApp(std::stoi(args, &index));
  args = args.substr(index);

  sprintf(name, "%s-%d-%d-%d", getFullPath().c_str(), p_msg->getSrcNode(),
          this->msgIdx, p_msg->getDstNode());
  p_msg->setName(name);
  p_msg->setInject_time(timestamp * timescale);
  p_msg->setSrcLid(getLid(p_msg->getSrcNode()));
  p_msg->setMsgId(this->msgIdx);
  p_msg->setDstLid(getLid(p_msg->getDstNode()));

  this->active.insert(std::make_pair(this->msgIdx,
                                     std::unique_ptr<DimReqMsg>(p_msg)));

  this->msgIdx++;

  return p_msg;
}

std::unique_ptr<DimReqMsg>
Controller::lookupMessage(unsigned int msgId)
{
    auto data_msg_iter = active.find(msgId);
    if (data_msg_iter == active.end()) {

        return nullptr;
    }
    auto our_ptr = std::move(data_msg_iter->second);
    active.erase(data_msg_iter);
    return our_ptr;
}

/** Called when a rendezvous request message corresponding to an MPI Send has
 * reached its destination.  Sends the RTR if there is one in the queue;
 * otherwise, enqueue this rendezvous request until a receive request is
 * posted. */
void Controller::matchRReq(DimReqMsg *rreq)
{
    auto key = make_pair(rreq->getSrcNode(), rreq->getDstNode());
    auto &queue = recvQueue[key];
    if (queue.empty()) {
        rreqCount[key]++;
    } else {
        DimReqMsg *rtr = queue.front();
        queue.pop();
        if (rtr->getKind() != IB_DIM_RTR_MSG) {
            throw cRuntimeError("Incorrect message kind %d in RTR queue for (%d, %d)",
                    rtr->getKind(), rreq->getSrcNode(), rreq->getDstNode());
        }
        if (rtr->getInject_time() < simTime()) {
            rtr->setInject_time(simTime());
        }
        sendMessage(rtr);
    }
}

/** Called when an RTR message corresponding to an MPI Recv has reached its
 * destination.  Sends the message data. */
void Controller::handleRTR(DimReqMsg *rtr)
{
    char name[128];
    DimReqMsg *send = rtr->dup();
    send->setSrcNode(rtr->getDstNode());
    send->setSrcLid(getLid(send->getSrcNode()));
    send->setDstNode(rtr->getSrcNode());
    send->setDstLid(getLid(send->getDstNode()));
    sprintf(name, "%s-%d-%d-%d", getFullPath().c_str(), send->getSrcNode(),
            this->msgIdx, send->getDstNode());
    send->setName(name);
    send->setInject_time(simTime());
    send->setKind(IB_DIM_SEND_MSG);
    send->setMsgId(this->msgIdx++);
    std::cerr << "Send real message at " << simTime() << '\n';
    this->active.insert(std::make_pair(send->getMsgId(),
                                         std::unique_ptr<DimReqMsg>(send)));
    sendMessage(send, true);
    ++rendezvousCount;
}

/** Called when a data message has reached its destination.  Sends the
 * completion notice to Dimemas. */
void Controller::handleSendCompletion(DimReqMsg *orig_msg,
                                      simtime_t when)
{
    std::ostringstream buf;
    buf << "COMPLETED SEND " << when.inUnit(SIMTIME_NS)
        << " " << orig_msg->getSrcNode()
        << " " << orig_msg->getDstNode()
        << " " << orig_msg->getLenBytes()
        << " " << std::hex << orig_msg->getEvent_ptr()
        << " " << std::hex << orig_msg->getEvent_out_ptr();
    std::string dimResponse = buf.str();
    std::cerr << "-I- " << getFullPath() << " Send to Dimemas: "
            << dimResponse << '\n';
    sock->sendLine(dimResponse + "\n");
    std::cerr << "-I- " << getFullPath() << " Send to Dimemas: END\n";
    sock->sendLine("END\n");

    /* We are returning control to Dimemas *now*, so cancel the STOP that we
     * just scheduled. */
    this->cancelEvent(&this->stopMessage);

    /* Let Dimemas react to the message completion. */
    this->waitOnDimemas();
}

void Controller::handleMessage(cMessage *p_msg){
  if (p_msg->isSelfMessage()) {
      if (p_msg != &stopMessage) {
          std::cerr << "Got unexpected self message:" << p_msg->getName();
          return;
      }
      std::string dimResponse{"STOP REACHED "};
      dimResponse.append(std::to_string(simTime().inUnit(SIMTIME_NS)));
      EV << "-I- " << getFullPath() << " Send to Dimemas: "
         << dimResponse << '\n';
      sock->sendLine(dimResponse + "\n");
      EV << "-I- " << getFullPath() << " Send to Dimemas: END\n";
      sock->sendLine("END\n");
      this->waitOnDimemas();
  } else if (p_msg->getKind() != IB_DIM_DONE_MSG) {
      std::cerr << "Got unexpected message kind " << p_msg->getKind()
              << '\n';
  } else {
      /* Get the completion message */
      auto done_msg = (DimDoneMsg *)p_msg;
      auto orig_msg = this->lookupMessage(done_msg->getMsgId());
      if (!orig_msg) {
          throw cRuntimeError("Got completion for unknown message %s\n", done_msg->getName());
      }

      switch (orig_msg->getKind()) {
      case IB_DIM_SEND_MSG:
          this->handleSendCompletion(orig_msg.get(), done_msg->getComplete_time());
          break;

      case IB_DIM_RREQ_MSG:
          this->matchRReq(orig_msg.get());
          break;

      case IB_DIM_RTR_MSG:
          this->handleRTR(orig_msg.get());
          break;

      default:
          abort();
      }

      delete p_msg;
  }
}

void Controller::finish()
{
  std::cerr << "Controller: eagerCount: " << eagerCount << "\n";
  std::cerr << "Controller: rendezvousCount: " << rendezvousCount << "\n";
  std::cerr << "Controller: totalCount: "
            << eagerCount + rendezvousCount << "\n";
  sock = nullptr;
}
