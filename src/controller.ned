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
package ib_model;

simple Controller
{
 parameters:
  string socketListenHost = "127.0.0.1"; // host to listen on
  string socketListenPort = "10000"; // port to listen on
  double dimemasTimescale @unit(s);
  double perMsgDelay @unit(ns);
  @display("i=block/telnet");
 gates:
  output out[];
  input done[];
}
