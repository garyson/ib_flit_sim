#!/usr/bin/env python3
# utils/make_topology.py
#
# InfiniBand FLIT (Credit) Level OMNet++ Simulation Model
#
# Copyright (c) 2016 University of New Hampshire InterOperability Laboratory
#
# This software is available to you under the terms of the GNU
# General Public License (GPL) Version 2, available from the file
# COPYING in the main directory of this source tree.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
# BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
# ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

from copy import copy
from jinja2 import Environment, FileSystemLoader, Template
import re
import sys
import yaml
try:
    from yaml import CLoader as Loader, CDumper as Dumper
except ImportError:
    from yaml import Loader, Dumper

'''
Parse an actual IB topology into the OMNet++/Dimemas equivalents.

A switch description looks like:

Switch	36 "S-0008f10500200134"		# "Voltaire 4036 # v4036" enhanced port 0 lid 1 lmc 0
[1]	"H-00117500007ea9b2"[1](117500007ea9b2) 		# "larissa HCA-1" lid 2 4xQDR
[2]	"H-00117500007eaa84"[1](117500007eaa84) 		# "triton HCA-1" lid 7 4xDDR
[4]	"H-001e0bffff4cafc0"[1](1e0bffff4cafc1) 		# "telesto HCA-1" lid 3 4xDDR
[5]	"H-0002c90300001dd0"[1](2c90300001dd1) 		# "skoll HCA-1" lid 8 4xDDR

A CA description looks like:

Ca	2 "H-001e0bffff4cafc0"		# "telesto HCA-1"
[1](1e0bffff4cafc1) 	"S-0008f10500200134"[4]		# lid 3 lmc 0 "Voltaire 4036 # v4036" lid 1 4xDDR

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


def normalize_guid(guid):
    """Normalize a GUID to contain exactly 16 characters, without the
    leading 0x.

    >>> normalize_guid('0x2c90300001dd0')
    0x0002c90300001dd0
    """
    return "0x" + format(int(guid, 16), "016x")


class HCA(yaml.YAMLObject):
    """Representation of a single HCA."""

    yaml_tag = '!HCA'

    def __init__(self, name):
        """Initializes the HCA."""
        self.name = 'H_' + name

    def set_lid(self, lid):
        """Set the LID of this HCA port to the given value."""
        self.lid = lid

    def set_guid(self, guid):
        """Set the GUID of this HCA port to the given value."""
        self.guid = normalize_guid(guid)

    def output_ned(self, outh):
        """Output the NED component for this HCA."""
        template = ("\t\t{{ name }}: HCA {\n\t\t\tparameters:\n"
                    "\t\t\t\tsrcLid = {{ lid }};\n"
                    "\t\t\t\tappCount = 1;\n\t\t}\n\n")
        outh.write(Template(template).render(name=self.name, lid=self.lid))

    def __repr__(self):
        return "{}(name={!r}, lid={!r}, guid={!r})".format(
                self.__class__.__name__, self.name, self.lid, self.guid)


class SwitchPort(yaml.YAMLObject):

    yaml_tag = '!SwitchPort'

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

    def __repr__(self):
        return "{}(remote={!r}, speed={!r}, orig_port_num={!r})".format(
                self.__class__.__name__, self.remote.name, self.speed,
                self.orig_port_num)


class Switch(yaml.YAMLObject):
    """Representation of a single switch."""

    yaml_tag = '!Switch'

    def __init__(self, guid):
        """Initialize the switch."""
        self.guid = normalize_guid(guid)
        self.ports = []
        self.fdb = []

    def set_name(self, name):
        """Set the human-readable name of the switch."""
        self.name = 'SW_' + name

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

    def __init__(self, config, hcas=[], switches=[], ranks=[]):
        """Initialize the network with the given name."""
        self.hcas = hcas
        self.switches = switches
        self.ranks = ranks
        self.config = config

    def find_hca(self, name):
        """Return the HCA with the given name.

        Return the HCA object or None if the HCA does not exist.

        """
        for hca in self.hcas:
            if hca.name == 'H_' + name:
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
            if switch.guid == normalize_guid(guid):
                return switch
        switch = Switch(guid)
        self.switches.append(switch)
        return switch

    @classmethod
    def _fixup_name(cls, name):
        return name.translate(str.maketrans({' ': '_', '#': '_',
            '-': '_', ';': '_', '/': '_', ':': '_'}))

    @classmethod
    def parse_ibnetdiscover_output(cls, name, filename):
        """Parse the output of ibnetdiscover into a Network object."""
        switch_re = re.compile(r'^Switch\s+(\d+)\s+"S-([0-9a-fA-F]{16})"\s+#\s+"([^"]+)"')
        hca_re = re.compile(r'^Ca\s+(\d+)\s+"H-([0-9a-fA-F]{16})"\s+#\s+"([^"]+)"')
        swport_re = re.compile(r'^\[(\d+)\]\s+"H-[0-9a-fA-F]{16}"\[\d+\]\([0-9a-fA-F]{1,16}\)\s+#\s+"([^"]+)"\s+lid\s+\d+\s+(\d+x[A-Z]{3})$')
        hcaport_re = re.compile(r'^\[\d+\]\([0-9a-fA-F]{1,16}\)\s+"S-[0-9a-fA-F]{16}"\[\d+\]\s+#\s+lid\s+(\d+)')

        res = Network(name)
        with open(filename) as inh:
            cur_switch = None
            cur_hca = None
            for line in inh:
                if cur_switch is not None:
                    m = swport_re.match(line.rstrip())
                    if m:
                        hcaname = cls._fixup_name(m.group(2))
                        port_hca = res.get_hca(cls._fixup_name(m.group(2)))
                        cur_switch.connect_hca(int(m.group(1)), port_hca,
                                               m.group(3).upper())
                    else:
                        cur_switch = None
                elif cur_hca is not None:
                    m = hcaport_re.match(line.rstrip())
                    if m:
                        cur_hca.set_lid(int(m.group(1)))
                    cur_hca = None
                else:
                    m = switch_re.match(line)
                    if m:
                        # Parse switch description
                        cur_switch = res.get_switch_by_guid(m.group(2))
                        cur_switch.set_name(cls._fixup_name(m.group(3)))
                    m = hca_re.match(line)
                    if m:
                        # Parse HCA description
                        cur_hca = res.get_hca(cls._fixup_name(m.group(3)))
                        cur_hca.set_guid(m.group(2))
        return res

    def parse_fdbs(self, filename):
        """Parse the filtering database output from ibdiagnet."""
        header_re = re.compile('Switch 0x([0-9A-Fa-f]{16})')
        with open(filename) as inh:
            switch = None
            fdb = [255]
            for line in inh:
                m = header_re.search(line)
                if m:
                    if switch is not None:
                        switch.fdb = fdb
                        fdb = [255]
                    switch = self.get_switch_by_guid(m.group(1))
                elif switch is not None:
                    if line.startswith('LID'):
                        continue
                    elif line.rstrip() == '':
                        switch.fdb = fdb
                        fdb = [255]
                        switch = None
                    else:
                        port = line.split(':')[1].strip()
                        if port == 'UNREACHABLE':
                            fdb.append(255)
                        else:
                            fdb.append(switch.find_port_by_orig_num(int(port)))
            if switch:
                switch.fdb = fdb

    def parse_ranktohost_csv(self, filename):
        """Parse the list of hosts accessible to MPI."""
        self.ranks = list()
        with open(filename) as inh:
            inh.readline()
            line = inh.readline()
            while line:
                hca = self.find_hca(line.rstrip().split(',')[1])
                if hca is None:
                    raise ValueError('hca ' + hca + ' not found')
                self.ranks.append(hca)
                line = inh.readline()

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
        with open('{}.fdbs'.format(self.config['name']), 'w') as outh:
            for swid, switch in enumerate(self.switches):
                outh.write('{}:'.format(swid))
                fdb = switch.fdb
                for x in fdb:
                    outh.write(' {}'.format(x))
                outh.write('\n')

    def _output_ned(self):
        """Output the OMNet++ NED file."""
        with open('{}.ned'.format(self.config['name']), 'w') as outh:
            outh.write('import ib_model.*;\n')
            outh.write('network {}\n'.format(self.config['name']))
            outh.write('{\n')
            outh.write('\tsubmodules:\n')
            for hca in self.hcas:
                hca.output_ned(outh)
            for switch in self.switches:
                switch.output_ned(outh)
            outh.write('\t\tcontroller: Controller {\n')
            outh.write('\t\t\tgates:\n')
            outh.write('\t\t\t\tout[{}];\n'.format(len(self.hcas)))
            outh.write('\t\t\t\tdone[{}];\n'.format(len(self.hcas)))
            outh.write('\t\t}\n')
            outh.write('\tconnections:\n')
            for switch in self.switches:
                for portid, port in enumerate(switch.ports):
                    outh.write('\t\t{}.port <--> IB{}Wire <--> {}.port[{}];\n'.format(
                        switch.get_port_remote(portid).name,
                        switch.get_port_speed(portid), switch.name, portid))
            for hcaid, hca in enumerate(self.hcas):
                outh.write('\t\tcontroller.out[{}] --> {}.msgIn[0];\n'.format(
                    hcaid, hca.name))
                outh.write('\t\t{}.msgDone --> controller.done[{}];\n'.format(
                    hca.name, hcaid))
            outh.write('}\n')

    def _output_ini(self):
        """Output the OMNet++ INI file."""
        base_dir = '/home/pmacarth/src/omnetpp-workspace/ib_model/utils'
        env = Environment(loader=FileSystemLoader(base_dir))
        template = env.get_template('omnetpp.ini.j2')
        with open('{}.ini'.format(self.config['name']), 'w') as outf:
            outf.write(template.render(name=self.config['name'],
                                       switches=self.switches))

    def _output_dimemas_cfg(self):
        """Output the dimemas CFG file."""
        base_dir = '/home/pmacarth/src/omnetpp-workspace/ib_model/utils'
        env = Environment(loader=FileSystemLoader(base_dir))
        template = env.get_template('dimemas.cfg.j2')
        with open('{}.dimemas.cfg'.format(self.config['name']), 'w') as outf:
            outf.write(template.render(network=self,
                processors_per_node=self.config['processors_per_node'],
                trace_file_name=self.config['trace_file']))

    def _output_yaml(self):
        output = dict()
        output['config'] = copy(self.config)
        if 'input' in output['config']:
            del output['config']['input']
        output['hcas'] = self.hcas
        output['ranks'] = self.ranks
        output['switches'] = self.switches
        with open('{}.network.yml'.format(self.config['name']), 'w') as outf:
            outf.write(yaml.dump(output, explicit_start=True))

    def output(self):
        """Output all configuration for the OMNet++ IB model."""
        self._output_ned()
        self._output_fdbs()
        self._output_ini()
        self._output_dimemas_cfg()
        self._output_yaml()


def main():
    with open(sys.argv[1]) as inf:
        dump = yaml.load(inf)
        config = dump['config']
        if 'input' in config:
            network = Network.parse_ibnetdiscover_output(config,
                    config['input']['ibnetdiscover_file'])
            network.parse_ranktohost_csv(config['input']['ranktohost_file'])
            network.parse_fdbs(config['input']['fdbs_file'])
        else:
            network = Network(config, hcas=dump['hcas'],
                              switches=dump['switches'],
                              ranks=dump['ranks'])
        network.output()

if __name__ == '__main__':
    main()
