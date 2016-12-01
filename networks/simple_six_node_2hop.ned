// networks/simple_six_node_2hop.ned
//
// InfiniBand FLIT (Credit) Level OMNet++ Simulation Model
//
// Copyright (c) 2004-2013 Mellanox Technologies, Ltd. All rights reserved.
// Copyright (c) 2016 University of New Hampshire InterOperability Laboratory
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

package ib_model.networks;

import ib_model.*;

network simple_six_node_2hop
{
    submodules:
        H_1: HCA {
            parameters:
                srcLid = 1;
                appCount = 16;
                @display("i=device/server2;p=80,109");
        }
        H_2: HCA {
            parameters:
                srcLid = 2;
                appCount = 16;
                @display("i=device/server2;p=80,191");
        }
        H_3: HCA {
            parameters:
                srcLid = 3;
                appCount = 16;
                @display("i=device/server2;p=80,250");
        }
        H_4: HCA {
            parameters:
                srcLid = 4;
                appCount = 16;
                @display("i=device/server2;p=80,300");
        }
        H_5: HCA {
            parameters:
                srcLid = 5;
                appCount = 16;
                @display("i=device/server2;p=80,350");
        }
        H_6: HCA {
            parameters:
                srcLid = 6;
                appCount = 1;
                @display("i=device/server2;p=80,400");
        }

        SW_1: Switch {
            parameters:
                numSwitchPorts = 6;
                @display("i=abstract/router;p=257,144");
            gates:
                port[6];
        }
        controller: Controller {
            parameters:
                @display("p=339,288");
            gates:
                out[6];
                done[6];
        }
    connections:
        H_1.port <--> IB4XFDRWire <--> SW_1.port[0];
        H_2.port <--> IB4XFDRWire <--> SW_1.port[1];
        H_3.port <--> IB4XFDRWire <--> SW_1.port[2];
	H_4.port <--> IB4XFDRWire <--> SW_1.port[3];
        H_5.port <--> IB4XFDRWire <--> SW_1.port[4];
        H_6.port <--> IB4XFDRWire <--> SW_1.port[5];

        controller.out[0] --> H_1.msgIn[0];
        controller.out[1] --> H_2.msgIn[0];
        controller.out[2] --> H_3.msgIn[0];
        controller.out[3] --> H_4.msgIn[0];
        controller.out[4] --> H_5.msgIn[0];
        controller.out[5] --> H_6.msgIn[0];
        H_1.msgDone --> controller.done[0];
        H_2.msgDone --> controller.done[1];
        H_3.msgDone --> controller.done[2];
        H_4.msgDone --> controller.done[3];
        H_5.msgDone --> controller.done[4];
        H_6.msgDone --> controller.done[5];
}
