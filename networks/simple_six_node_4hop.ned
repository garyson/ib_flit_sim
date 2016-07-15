package ib_model.networks;

import ib_model.*;

network simple_six_node
{
    submodules:
        H_1: HCA {
            parameters:
                srcLid = 1;
                appCount = 1;
                @display("i=device/server2;p=80,109");
        }
        H_2: HCA {
            parameters:
                srcLid = 2;
                appCount = 1;
                @display("i=device/server2;p=80,191");
        }
        H_3: HCA {
            parameters:
                srcLid = 3;
                appCount = 1;
                @display("i=device/server2;p=80,250");
        }
        H_4: HCA {
            parameters:
                srcLid = 4;
                appCount = 1;
                @display("i=device/server2;p=80,300");
        }
        H_5: HCA {
            parameters:
                srcLid = 5;
                appCount = 1;
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
                numSwitchPorts = 4;
                @display("i=abstract/router;p=257,144");
            gates:
                port[4];
        }
        SW_2: Switch {
            parameters:
                numSwitchPorts = 2;
                @display("i=abstract/router;p=257,200");
            gates:
                port[2];
        }
        SW_3: Switch {
            parameters:
                numSwitchPorts = 4;
                @display("i=abstract/router;p=257,250");
            gates:
                port[4];
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
        SW_2.port[0] <--> IB4XFDRWire <--> SW_1.port[3];
	H_4.port <--> IB4XFDRWire <--> SW_3.port[0];
        H_5.port <--> IB4XFDRWire <--> SW_3.port[1];
        H_6.port <--> IB4XFDRWire <--> SW_3.port[2];
        SW_2.port[1] <--> IB4XFDRWire <--> SW_3.port[3];

        controller.out[0] --> H_1.msgIn[0];
        H_1.msgDone --> controller.done[0];

        controller.out[1] --> H_2.msgIn[0];
        H_2.msgDone --> controller.done[1];

        controller.out[2] --> H_3.msgIn[0];
        H_3.msgDone --> controller.done[2];

        controller.out[3] --> H_4.msgIn[0];
        H_4.msgDone --> controller.done[3];

        controller.out[4] --> H_5.msgIn[0];
        H_5.msgDone --> controller.done[4];

        controller.out[5] --> H_6.msgIn[0];
        H_6.msgDone --> controller.done[5];
}
