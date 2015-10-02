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
// An Application - Message Generator
//
// Overview:
// =========
// The task of the IB message generator is to mimic an application
//
// Each message has several packet towards a single destination.
//
// Message generation can be configured with the following set of orthogonal
// mechanisms:
// * Destination selection - how a message destinaton is defined
// * Message length - what length the message will be in (can be down
//   to packet length if we are in single packet message
// * Injection rate and burstiness - we incorporate inter/intra message jitter
// * SQ - what SQ the application messages belongs to
//
// The mechanisms above are all orthogonal so the user needs to select
// a combination of each mechanism to fully configure the generator
//
// Connectivity:
// =============
// OUT: out port - sending "appMsg" events
// IN:  done port - receiving "done" events
//
// Events:
// =======
// New messages are generated when the previous one is consumed and
// reported as "done" on the done port.
// No internal messages are required
//
// Destination Selection:
// ======================
// We provide the following modes for message destination selection:
// DST_PARAM - destination set by dstLid parameter
// DST_SEQ_* - A sequence of destinations is provided. This mode have further
//           sub-modes defined by DST_SEQ_MODE which may be:
//           DST_SEQ_ONCE - go over the sequence only once - flag completion
//           DST_SEQ_LOOP - loop over the sequence in
//           DST_SEQ_RAND - choose from the sequence in random order
//
// Parameters for destination selection:
// dstMode - possible values: param|seq_once|seq_loop|seq_rand
// dstLid - the destination LID - used in DST_PARAM
// dstSeqVecFile - the vector file name that contain the sequences
// dstSeqVecIdx - the index of the generator in the file
//
// Message/Packet Size Selection:
// ==============================
// MSG_LEN_PARAM - message length is based on msgSize param
// MSG_LEN_SET - selects from a set of sizes with their relative probability
//
// Parameters for message size:
// msgLenMode - possible values: param|set
// msgLength_B - the length of a message in bytes - last packet may be padded
// msgLenSet - a set of lengths
// msgLenProb - probability for each length
// mtuLen_B - the MTU of single packet. It is the same for entire message.
//
// NOTE: due to current limitation of the simulator of sending full flits
// all sizes are padded to flitSize ...
//
// Traffic Shaping:
// ================
// There are no special modes here.
//
// Parameters that control shaping:
// msg2msgGap_ns - the extra delay from one msg end to the next start [ns]
//
// SQ selection:
// ================
// Currently there is nothing special here. SQ assigned by param
// for every new message
//
// parameters
// maxSQ - the value of the maximal SQ
// msgSQ - the SQ to be used for the message
//

#ifndef __APP_H
#define __APP_H

#include <omnetpp.h>

#include <map>
#include <memory>

#include "socket.h"

using DimMessageID = unsigned int;
using MessageTable = std::map<DimMessageID, std::unique_ptr<DimReqMsg> >;

/* A simple structure to uniquely identify a queue pair used by an instance of
 * the simulated application. */
struct QPIdentifier {
    int lid;
    int qpn;

    QPIdentifier(int l, int q) : lid(l), qpn(q) {}
};

//
// Generates IB Application Messages
//
class Controller : public cSimpleModule
{
 private:
  // Identify each message in printable strings
  unsigned int msgIdx;

  // Server socket used for communication with Dimemas
  std::unique_ptr<Socket> sock;

  std::vector<QPIdentifier> rankMapping;

  // Stop message used to return control to Dimemas
  cMessage stopMessage;

  // Active message table; used to resolve completions
  MessageTable active;

  // Dimemas timescale
  double timescale;

  // methods
 private:

  void waitOnDimemas();

  bool handleDimemasFinish(std::string args);
  bool handleDimemasEnd(std::string args);
  bool handleDimemasStop(std::string args);
  bool handleDimemasSend(std::string args);

  unsigned int rank2lid(unsigned int rank) { return rankMapping[rank].lid; }
  unsigned int rank2qpn(unsigned int rank) { return rankMapping[rank].qpn; }

  // Initialize a new set of parameters for a new message
  void startSendRecv(double timestamp,
                      unsigned int msgSrcLid, unsigned int msgDstLid,
                      unsigned int msgLen_B, std::string dimemasName);

  std::unique_ptr<DimReqMsg> lookupMessage(unsigned int msgId);

 protected:
  virtual void initialize();
  virtual void handleMessage(cMessage *msg);
  virtual void finish();
};

#endif
