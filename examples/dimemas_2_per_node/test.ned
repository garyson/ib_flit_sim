// examples/dimemas_2_per_node/test.ned
//
// InfiniBand FLIT (Credit) Level OMNet++ Simulation Model
//
// Copyright (c) 2004-2013 Mellanox Technologies, Ltd. All rights reserved.
// Copyright (c) 2015-2016 University of New Hampshire InterOperability Laboratory
//
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

package ib_model.examples.dimemas_2_per_node;

import ib_model.*;

network test
{
    submodules:
        H_1: HCA {
            parameters:
                srcLid = 1;
		appCount = 2;
                @display("i=device/server2;p=80,109");
        }
        H_2: HCA {
            parameters:
                srcLid = 2;
		appCount = 2;
                @display("i=device/server2;p=80,191");
        }

        SW_1: Switch {
            parameters:
                numSwitchPorts = 2;
                @display("i=abstract/router;p=257,144");
            gates:
                port[2];
        }
        controller: Controller {
            parameters:
                @display("p=339,288");
            gates:
                out[4];
                done[2];
        }
    connections:
        H_1.port <--> IB4XQDRWire <--> SW_1.port[0];
        H_2.port <--> IB4XQDRWire <--> SW_1.port[1];

        controller.out[0] --> H_1.msgIn[0];
        controller.out[1] --> H_1.msgIn[1];
        H_1.msgDone --> controller.done[0];
        controller.out[2] --> H_2.msgIn[0];
        controller.out[3] --> H_2.msgIn[1];
        H_2.msgDone --> controller.done[1];
}
