package ib_model.examples.is1_1l_8n;

import ib_model.*;

network is1_1l_8n
{
	submodules:
		H_1_U1: HCA { parameters: srcLid = 1; }
		H_2_U1: HCA { parameters: srcLid = 2; }
		H_3_U1: HCA { parameters: srcLid = 3; }
		H_4_U1: HCA { parameters: srcLid = 4; }
		H_5_U1: HCA { parameters: srcLid = 5; }
		H_6_U1: HCA { parameters: srcLid = 6; }
		H_7_U1: HCA { parameters: srcLid = 7; }
		H_8_U1: HCA { parameters: srcLid = 8; }
		SW_L0_0_0_U1: Switch {
		parameters: numSwitchPorts = 8;
			gates: port[8];
		}
	connections:
		H_1_U1.port <--> IB4XQDRWire <--> SW_L0_0_0_U1.port[0];
		H_2_U1.port <--> IB4XQDRWire <--> SW_L0_0_0_U1.port[1];
		H_3_U1.port <--> IB4XQDRWire <--> SW_L0_0_0_U1.port[2];
		H_4_U1.port <--> IB4XQDRWire <--> SW_L0_0_0_U1.port[3];
		H_5_U1.port <--> IB4XQDRWire <--> SW_L0_0_0_U1.port[4];
		H_6_U1.port <--> IB4XQDRWire <--> SW_L0_0_0_U1.port[5];
		H_7_U1.port <--> IB4XQDRWire <--> SW_L0_0_0_U1.port[6];
		H_8_U1.port <--> IB4XQDRWire <--> SW_L0_0_0_U1.port[7];
}
