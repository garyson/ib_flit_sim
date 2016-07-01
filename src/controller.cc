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

#if defined(__GNUC__)
#define UNUSED __attribute__((__unused__))
#else
#define UNUSED
#endif

Define_Module(Controller);

namespace {
using DimemasHandler = bool (Controller::*)(std::string);

/* Trims whitespace from the beginning and end of the given string. */
std::string
trim_string(std::string in)
{
    const unsigned int size = in.size();
    unsigned int f, l;
    for (f = 0; f < size && std::isspace(in[f]); ++f) {
    }
    for (l = size - 1; l >= f && std::isspace(in[l]); --l) {
    }
    return in.substr(f, l - f + 1);
}
}

// main init of the module
void Controller::initialize(){
  timescale = par("dimemasTimescale");
  stopMessage.setSchedulingPriority(SHRT_MAX);

  const char *socketListenHost = par("socketListenHost");
  const char *socketListenPort = par("socketListenPort");
  const char *ranksFile = "ranks.txt";
  if (hasPar("ranksFile")) {
      ranksFile = par("ranksFile");
  }

  try {
      ifstream f{ranksFile};
      std::string line;
      while (getline(f, line)) {
          std::istringstream ss(line);
          int lid;
          int qpnum;
          ss >> lid >> qpnum;
          if (!ss) {
              throw std::runtime_error("invalid rank identifier");
          }
          this->rankMapping.emplace_back(lid, qpnum);
      }
      if (!f.eof()) {
          std::ostringstream os;
          os << "Error reading rank to LID mappings from "
             << ranksFile << "\n";
          throw std::runtime_error(os.str());
      }
  } catch (std::exception &ex) {
      std::cerr << ex.what() << '\n';
      throw;
  }

  try {
  // start listening socket
  ListeningSocket listener(socketListenHost, socketListenPort);
  listener.listen();
  std::cerr << "-I- " << getFullPath()
     << " Listening for Dimemas VenusClient connection on "
     << listener.getName() << '\n';

  sock = std::unique_ptr<Socket>(new Socket(std::move(listener.accept())));
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
    size_t index = 0;
    double timestamp = std::stod(args, &index);
    args = args.substr(index);

    index = 0;
    unsigned int srcRank = std::stoi(args, &index);
    args = args.substr(index);

    index = 0;
    unsigned int dstRank = std::stoi(args, &index);
    args = args.substr(index);

    index = 0;
    UNUSED unsigned int tag = std::stoi(args, &index);
    args = args.substr(index);

    index = 0;
    unsigned int msgSize = std::stoi(args, &index);
    args = trim_string(args.substr(index));

    // generate a new message
    auto p_msg = makeMessage(timestamp, IB_DIM_RREQ_MSG, srcRank, dstRank,
                             msgSize, args);
    sendMessage(p_msg);

    return true;
}

/** Handle a PROTO READY_TO_RECV message received from Dimemas. */
bool Controller::handleDimemasRTR(std::string args)
{
    size_t index = 0;
    double timestamp = std::stod(args, &index);
    args = args.substr(index);

    index = 0;
    unsigned int srcRank = std::stoi(args, &index);
    args = args.substr(index);

    index = 0;
    unsigned int dstRank = std::stoi(args, &index);
    args = args.substr(index);

    index = 0;
    UNUSED unsigned int tag = std::stoi(args, &index);
    args = args.substr(index);

    index = 0;
    unsigned int msgSize = std::stoi(args, &index);
    args = trim_string(args.substr(index));

    // generate a new message
    auto p_msg = makeMessage(timestamp, IB_DIM_RTR_MSG, dstRank, srcRank,
                             msgSize, args);

    auto iter = rreqCount.find(make_pair(srcRank, dstRank));
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
    size_t index = 0;
    double timestamp = std::stod(args, &index);
    args = args.substr(index);

    index = 0;
    unsigned int srcRank = std::stoi(args, &index);
    args = args.substr(index);

    index = 0;
    unsigned int dstRank = std::stoi(args, &index);
    args = args.substr(index);

    index = 0;
    UNUSED unsigned int tag = std::stoi(args, &index);
    args = args.substr(index);

    index = 0;
    unsigned int msgSize = std::stoi(args, &index);
    args = trim_string(args.substr(index));

    // generate a new message
    ++eagerCount;
    auto p_msg = makeMessage(timestamp, IB_DIM_SEND_MSG, srcRank, dstRank,
                             msgSize, args);
    sendMessage(p_msg);

    return true;
}

/** Place a new message from Dimemas at the end of the appropriate receive
 * queue. */
void Controller::enqueueRTR(DimReqMsg *req)
{
    auto key = make_pair(req->getDstRank(), req->getSrcRank());
    auto &queue = recvQueue[key];
    queue.push(req);
}

/** Sends a message to the application object indicated by the source rank. */
void Controller::sendMessage(DimReqMsg *p_msg)
{
    simtime_t delay = p_msg->getInject_time();
    simtime_t cur = simTime();
    if (delay >= cur) {
        sendDelayed(p_msg, delay - cur, "out", p_msg->getSrcRank());
    } else {
        /* Due to rounding error, the delay for this message would be negative.
         *   Send immediately to avoid OMNet++ errors. */
        send(p_msg, "out", p_msg->getSrcRank());
    }

    EV << "-I- " << getFullPath()
       << " sending new controller message " << p_msg->getName()
       << endl;
}

/** Creates a message object given the parameters. */
DimReqMsg *Controller::makeMessage(double timestamp, IB_MSGS msgType,
                           unsigned int msgSrcRank, unsigned int msgDstRank,
                           unsigned int msgLen_B, std::string dimemasName)
{
  DimReqMsg *p_msg;
  char name[128];
  sprintf(name, "%s-%d-%d", getFullPath().c_str(), msgSrcRank, this->msgIdx);
  p_msg = new DimReqMsg(name, msgType);
  p_msg->setInject_time(timestamp * timescale);
  p_msg->setSrcRank(msgSrcRank);
  p_msg->setSrcLid(rank2lid(msgSrcRank));
  p_msg->setMsgId(this->msgIdx);
  p_msg->setDstRank(msgDstRank);
  p_msg->setDstLid(rank2lid(msgDstRank));
  p_msg->setLenBytes(msgLen_B);
  p_msg->setContextString(dimemasName.c_str());

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
    auto key = make_pair(rreq->getSrcRank(), rreq->getDstRank());
    auto &queue = recvQueue[key];
    if (queue.empty()) {
        rreqCount[key]++;
    } else {
        DimReqMsg *rtr = queue.front();
        queue.pop();
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
    DimReqMsg *send = makeMessage(simTime().dbl(), IB_DIM_SEND_MSG,
                                  rtr->getDstRank(), rtr->getSrcRank(),
                                  rtr->getLenBytes(),
                                  rtr->getContextString());
    sendMessage(send);
    ++rendezvousCount;
}

/** Called when a data message has reached its destination.  Sends the
 * completion notice to Dimemas. */
void Controller::handleSendCompletion(DimReqMsg *orig_msg,
                                      simtime_t when)
{
    std::string dimResponse{"COMPLETED SEND "};
    dimResponse.append(std::to_string(when.inUnit(-9)));
    dimResponse.append(" ");
    dimResponse.append(std::to_string(orig_msg->getSrcRank()));
    dimResponse.append(" ");
    dimResponse.append(std::to_string(orig_msg->getDstRank()));
    dimResponse.append(" ");
    dimResponse.append(std::to_string(orig_msg->getLenBytes()));
    dimResponse.append(" ");
    dimResponse.append(orig_msg->getContextString());
    EV << "-I- " << getFullPath() << " Send to Dimemas: "
            << dimResponse << '\n';
    sock->sendLine(dimResponse + "\n");
    EV << "-I- " << getFullPath() << " Send to Dimemas: END\n";
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
      dimResponse.append(std::to_string(simTime().inUnit(-9)));
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
      auto orig_msg = std::move(this->lookupMessage(done_msg->getMsgId()));
      if (!orig_msg) {
          std::cerr << "Got completion for unknown message "
                    << done_msg->getName() << '\n';
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
