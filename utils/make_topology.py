#!/usr/bin/env python3

from jinja2 import Environment, FileSystemLoader, Template
import re

'''
A switch description looks like:

Switch	36 "S-0008f10500200134"		# "Voltaire 4036 # v4036" enhanced port 0 lid 1 lmc 0
[1]	"H-00117500007ea9b2"[1](117500007ea9b2) 		# "larissa HCA-1" lid 2 4xQDR
[2]	"H-00117500007eaa84"[1](117500007eaa84) 		# "triton HCA-1" lid 7 4xDDR
[4]	"H-001e0bffff4cafc0"[1](1e0bffff4cafc1) 		# "telesto HCA-1" lid 3 4xDDR
[5]	"H-0002c90300001dd0"[1](2c90300001dd1) 		# "skoll HCA-1" lid 8 4xDDR

A CA description looks like:

Ca	2 "H-001e0bffff4cafc0"		# "telesto HCA-1"
[1](1e0bffff4cafc1) 	"S-0008f10500200134"[4]		# lid 3 lmc 0 "Voltaire 4036 # v4036" lid 1 4xDDR

Need to parse out the Switch or CA line, and then the list of each port.
Not all ports are going to be connected; only the ports that are are
listed.

Internal data structures:

    hcas: list of HCAs
       - name
       - lid
       - guid (16 byte hex string)
    switches: list of switches, contains connections to HCAs
       - name
       - ports:
         - remote (string: name of HCA)
         - speed
       - guid
    ranks: list of MPI ranks, each is on an HCA

We need to generate the equivalent NED file.
'''


class HCA:
    """Representation of a single HCA."""

    def __init__(self, name):
        """Initializes the HCA."""
        self.name = name
        self.appid = 0

    def set_lid(self, lid):
        """Set the LID of this HCA port to the given value."""
        self.lid = lid

    def set_guid(self, guid):
        """Set the GUID of this HCA port to the given value."""
        self.guid = guid

    def output_ned(self, outh):
        """Output the NED component for this HCA."""
        template = ("\t\t{{ name }}: HCA {\n\t\t\tparameters:\n"
                    "\t\t\t\tsrcLid = {{ lid }};\n"
                    "\t\t\t\tappCount = 1;\n\t\t}\n\n")
        outh.write(Template(template).render(name=self.name, lid=self.lid))


class SwitchPort:
    def __init__(self, hca, speed, portid):
        """Initialize the port.

        Initialize the port with the given remote endpoint, speed,
        and the given remote port number (not used by the model but
        instead used to parse the original filtering database output by
        the SM).

        """
        self.remote = hca
        self.speed = speed
        self.orig_port_num = portid


class Switch:
    """Representation of a single switch."""

    def __init__(self, guid):
        """Initialize the switch."""
        self.guid = guid
        self.ports = []
        self.fdbs = []

    def set_name(self, name):
        """Set the human-readable name of the switch."""
        self.name = name

    def connect_hca(self, portid, hca, speed):
        """Connect the given HCA at the given port number and speed."""
        self.ports.append(SwitchPort(hca, speed, portid))

    def get_port_remote(self, portnum):
        """Return the remote HCA at the given port number."""
        return self.ports[portnum].remote

    def get_port_speed(self, portnum):
        """Return the link speed at the given port number."""
        return self.ports[portnum].speed

    def find_port_by_orig_num(self, orig_port):
        """Return the HCA connected at the original port number."""
        for portid, port in enumerate(self.ports):
            if port.orig_port_num == orig_port:
                return portid
        return 255

    def output_ned(self, outh):
        """Output the NED component for this switch."""
        template = ("\t\t{{ name }}: Switch {\n\t\t\tparameters:\n"
                    "\t\t\t\tnumSwitchPorts = {{ portCount }};\n"
                    "\t\t\tgates:\n\t\t\t\tport[{{ portCount }}];\n\t\t}\n\n")
        outh.write(Template(template).render(name=self.name,
                   portCount=len(self.ports)))


class Network:

    """Representation of an IB network consisting of HCAs and
    switches.

    """

    def __init__(self, name):
        """Initialize the network with the given name."""
        self.hcas = []
        self.switches = []
        self.ranks = []
        self.name = name

    def find_hca(self, name):
        """Return the HCA with the given name.

        Return the HCA object or None if the HCA does not exist.

        """
        for hca in self.hcas:
            if hca.name == name:
                return hca
        return None

    def get_hca(self, name):
        """Return and/or create the HCA with the given name."""
        hca = self.find_hca(name)
        if hca is not None:
            return hca
        hca = HCA(name)
        self.hcas.append(hca)
        return hca

    def get_switch_by_guid(self, guid):
        """Return and/or create the switch with the given GUID."""
        for switch in self.switches:
            if switch.guid == guid:
                return switch
        switch = Switch(guid)
        self.switches.append(switch)
        return switch

    @classmethod
    def parse_ibnetdiscover_output(cls, name, filename):
        """Parse the output of ibnetdiscover into a Network object."""
        switch_re = re.compile(r'^Switch\s+(\d+)\s+"S-([0-9a-fA-F]{16})"\s+#\s+"([^#]*#\s+)?([^"]+)"')
        hca_re = re.compile(r'^Ca\s+(\d+)\s+"H-([0-9a-fA-F]{16})"\s+#\s+"(\w+) HCA')
        swport_re = re.compile(r'^\[(\d+)\]\s+"H-[0-9a-fA-F]{16}"\[\d+\]\([0-9a-fA-F]{1,16}\)\s+#\s+"(\w+)\s+HCA-\d+"\s+lid\s+\d+\s+(\d+x[A-Z]{3})$')
        hcaport_re = re.compile(r'^\[\d+\]\([0-9a-fA-F]{1,16}\)\s+"S-[0-9a-fA-F]{16}"\[\d+\]\s+#\s+lid\s+(\d+)')

        res = Network(name)
        with open(filename) as inh:
            cur_switch = None
            cur_hca = None
            for line in inh:
                if cur_switch is not None:
                    m = swport_re.match(line.rstrip())
                    if m:
                        port_hca = res.get_hca(m.group(2))
                        cur_switch.connect_hca(int(m.group(1)), port_hca,
                                               m.group(3).upper())
                    else:
                        cur_switch = None
                elif cur_hca is not None:
                    m = hcaport_re.match(line.rstrip())
                    if m:
                        cur_hca.set_lid(m.group(1))
                    cur_hca = None
                else:
                    m = switch_re.match(line)
                    if m:
                        # Parse switch description
                        cur_switch = res.get_switch_by_guid(m.group(2))
                        cur_switch.set_name(m.group(4))
                    m = hca_re.match(line)
                    if m:
                        # Parse HCA description
                        cur_hca = res.get_hca(m.group(3))
                        cur_hca.set_guid(m.group(2))
        return res

    def parse_fdbs(self, filename):
        """Parse the filtering database output from ibdiagnet."""
        header_re = re.compile('Switch 0x([0-9A-Fa-f]{16})')
        with open(filename) as inh:
            switch = None
            fdbs = [255]
            for line in inh:
                m = header_re.search(line)
                if m:
                    if switch is not None:
                        switch.fdbs = fdbs
                        fdbs = [255]
                    switch = self.get_switch_by_guid(m.group(1))
                elif switch is not None:
                    if line.startswith('LID'):
                        continue
                    elif line.rstrip() == '':
                        switch.fdbs = fdbs
                        fdbs = [255]
                        switch = None
                    else:
                        port = line.split(':')[1].strip()
                        if port == 'UNREACHABLE':
                            fdbs.append(255)
                        else:
                            fdbs.append(
                                    switch.find_port_by_orig_num(int(port)))

    def parse_ranktohost_csv(self, filename):
        """Parse the list of hosts accessible to MPI."""
        self.ranks = list()
        with open(filename) as inh:
            inh.readline()
            line = inh.readline()
            while line:
                hca = self.get_hca(line.rstrip().split(',')[1])
                self.ranks.append(hca)
                line = inh.readline()
        return self.ranks

    def _output_sddf_rank_array(self):
        res = ""
        for rank in self.ranks:
            for hcaid, hca in enumerate(self.hcas):
                if hca == rank:
                    res += str(hcaid) + ','
                    continue
        return res[0:-1]

    def _output_fdbs(self):
        """Output the filtering database in format for OMNet++ IB model."""
        with open('{}.fdbs'.format(self.name), 'w') as outh:
            for swid, switch in enumerate(self.switches):
                outh.write('{}:'.format(swid))
                fdbs = switch.fdbs
                for x in fdbs:
                    outh.write(' {}'.format(x))
                outh.write('\n')

    def _output_ned(self):
        """Output the OMNet++ NED file."""
        with open('{}.ned'.format(self.name), 'w') as outh:
            outh.write('package ib_model.networks;\n')
            outh.write('import ib_model.*;\n')
            outh.write('network {}\n'.format(self.name))
            outh.write('{\n')
            outh.write('\tsubmodules:\n')
            for hca in self.hcas:
                hca.output_ned(outh)
            for switch in self.switches:
                switch.output_ned(outh)
            outh.write('\t\tcontroller: Controller {\n')
            outh.write('\t\t\tgates:\n')
            rankset = set()
            for rank in self.ranks:
                rankset.add(rank)
            outh.write('\t\t\t\tout[{}];\n'.format(len(rankset)))
            outh.write('\t\t\t\tdone[{}];\n'.format(len(rankset)))
            outh.write('\t\t}\n')
            outh.write('\tconnections:\n')
            for switch in self.switches:
                for portid, port in enumerate(switch.ports):
                    outh.write('\t\t{}.port <--> IB{}Wire <--> {}.port[{}];\n'.format(
                        switch.get_port_remote(portid).name,
                        switch.get_port_speed(portid), switch.name, portid))
            for rank, hca in enumerate(rankset):
                outh.write('\t\tcontroller.out[{}] --> {}.msgIn[0];\n'.format(
                    rank, hca.name))
                outh.write('\t\t{}.msgDone --> controller.done[{}];\n'.format(
                    hca.name, rank))
            outh.write('}\n')

    def _output_ini(self):
        """Output the OMNet++ INI file."""
        base_dir = '/home/pmacarth/src/omnetpp-workspace/ib_model/utils'
        env = Environment(loader=FileSystemLoader(base_dir))
        template = env.get_template('omnetpp.ini.j2')
        with open('{}.ini'.format(self.name), 'w') as outf:
            outf.write(template.render(name=self.name, switches=self.switches))

    def _output_dimemas_cfg(self):
        """Output the dimemas CFG file."""
        base_dir = '/home/pmacarth/src/omnetpp-workspace/ib_model/utils'
        env = Environment(loader=FileSystemLoader(base_dir))
        template = env.get_template('dimemas.cfg.j2')
        with open('{}.dimemas.cfg'.format(self.name), 'w') as outf:
            outf.write(template.render(network=self, processors_per_node=16))

    def output(self):
        """Output all configuration for the OMNet++ IB model."""
        self._output_ned()
        self._output_fdbs()
        self._output_ini()
        self._output_dimemas_cfg()


def main():
    network = Network.parse_ibnetdiscover_output('test', 'ibnetdiscover.out')
    network.parse_ranktohost_csv('ranktohost.csv')
    network.parse_fdbs('ibdiagnet.fdbs')
    network.output()

if __name__ == '__main__':
    main()
