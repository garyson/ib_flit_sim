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
