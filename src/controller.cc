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

  try {
      ifstream f{"ranks.txt"};
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

  this->waitOnDimemas();
}

void Controller::waitOnDimemas()
{
    static std::map<std::string, DimemasHandler> table = {
        {"SEND", &Controller::handleDimemasSend},
        {"STOP", &Controller::handleDimemasStop},
        {"END", &Controller::handleDimemasEnd},
        {"FINISH", &Controller::handleDimemasFinish},
    };

    while (true) {
        std::string line = sock->recvLine();
        if (line.empty()) {
            return;
        }
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
    EV << "-I- " << getFullPath() << " Got Dimemas FINISH";
    /* This will throw an exception so this method never actually
     * returns. */
    this->endSimulation();
    return false;
}

bool Controller::handleDimemasEnd(std::string args)
{
    EV << "-I- " << getFullPath() << " Got Dimemas END " << args;
    return false;
}

bool Controller::handleDimemasStop(std::string args)
{
    EV << "-I- " << getFullPath() << " Got Dimemas STOP " << args;

    double timestamp = std::stod(args);
    simtime_t delay{timestamp * timescale};
    simtime_t cur = simTime();
    if (delay < cur) {
        delay = cur;
    }
    scheduleAt(delay, &this->stopMessage);
    return true;
}

bool Controller::handleDimemasSend(std::string args)
{
    EV << "-I- " << getFullPath() << " Got Dimemas SEND " << args;

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
    startSendRecv(timestamp, srcRank, dstRank, msgSize, args);



    return true;
}

// Initialize the parameters for a new message by sampling the
// relevant parameters and  allocate and init a new message
void Controller::startSendRecv(double timestamp,
                           unsigned int msgSrcRank, unsigned int msgDstRank,
                           unsigned int msgLen_B, std::string dimemasName)
{
  DimReqMsg *p_msg;
  char name[128];
  sprintf(name, "%s-%d-%d", getFullPath().c_str(), msgSrcRank, this->msgIdx);
  p_msg = new DimReqMsg(name, IB_DIM_REQ_MSG);
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

  simtime_t delay{timestamp * timescale};
  simtime_t cur = simTime();
  if (delay >= cur) {
      sendDelayed(p_msg, delay - cur, "out", msgSrcRank);
  } else {
      /* Due to rounding error, the delay for this message would be negative.
       *   Send immediately to avoid OMNet++ errors. */
      send(p_msg, "out", msgSrcRank);
  }

  EV << "-I- " << getFullPath()
     << " sending new controller message " << p_msg->getName()
     << endl;
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

void Controller::handleMessage(cMessage *p_msg){
  if (p_msg->isSelfMessage()) {
      if (p_msg != &stopMessage) {
          std::cerr << "Got unexpected self message:" << p_msg->getName();
          return;
      }
      std::string dimResponse{"STOP REACHED "};
      dimResponse.append(std::to_string(simTime().dbl() / timescale));
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

      std::string dimResponse{"COMPLETED SEND "};
      dimResponse.append(std::to_string(done_msg->getComplete_time().dbl()
              / timescale));
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

      delete p_msg;
  }
}

void Controller::finish()
{
  sock = nullptr;
}
