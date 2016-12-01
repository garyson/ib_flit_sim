///////////////////////////////////////////////////////////////////////////
//
//         InfiniBand FLIT (Credit) Level OMNet++ Simulation Model
//
// Copyright (c) 2004-2013 Mellanox Technologies, Ltd. All rights reserved.
// Copyright (c) 2014,2016 University of New Hampshire InterOperability Laboratory
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
package ib_model;

simple IBGenerator
{
 parameters:
  int srcLid; // the generator LID
  int flitSize @unit(B) = default(64B); // number of bytes in each credit/FLIT
  int maxContPkts = default(10); // maximal continoues packets for msg
  double genDlyPerByte @unit(ns); // the speed of generating new data
  double popDlyPerByte @unit(ns) = default(0ns); // the speed of sending to VLA
  int maxQueuedPerVL = default(4); // the maximal outstanding FLITs in Q per VL

  // - shape
  volatile double flit2FlitGap @unit(ns); // extra delay between flits

  // - SQ to VL TBD
  int maxVL;

  int maxInlineData @unit(B) = default(0B); // maximum data inline with WR
 gates:
  input sent;
  output out;
  inout  in[]; // where app messages comes in
}
