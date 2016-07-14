#!/usr/bin/env python3

from jinja2 import Environment, FileSystemLoader
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
       - appCount
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


def parse_ibnetdiscover_output(filename):
    switch_re = re.compile(r'^Switch\s+(\d+)\s+"S-([0-9a-fA-F]{16})"\s+#\s+"([^#]*#\s+)?([^"]+)"')
    hca_re = re.compile(r'^Ca\s+(\d+)\s+"H-([0-9a-fA-F]{16})"\s+#\s+"(\w+) HCA')
    swport_re = re.compile(r'^\[\d+\]\s+"H-[0-9a-fA-F]{16}"\[\d+\]\([0-9a-fA-F]{1,16}\)\s+#\s+"(\w+)\s+HCA-\d+"\s+lid\s+\d+\s+(\d+x[A-Z]{3})$')
    hcaport_re = re.compile(r'^\[\d+\]\([0-9a-fA-F]{1,16}\)\s+"S-[0-9a-fA-F]{16}"\[\d+\]\s+#\s+lid\s+(\d+)')

    hcas = []
    switches = []
    with open(filename) as inh:
        cur_switch = None
        cur_hca = None
        for line in inh:
            if cur_switch is not None:
                m = swport_re.match(line.rstrip())
                if m:
                    cur_switch['ports'].append({'remote': m.group(1),
                                                'speed': m.group(2).upper()})
                else:
                    switches.append(cur_switch)
                    cur_switch = None
            elif cur_hca is not None:
                m = hcaport_re.match(line.rstrip())
                if m:
                    cur_hca['lid'] = m.group(1)
                    hcas.append(cur_hca)
                cur_hca = None
            else:
                m = switch_re.match(line)
                if m:
                    # Parse switch description
                    cur_switch = dict()
                    cur_switch['name'] = m.group(4)
                    cur_switch['guid'] = m.group(2)
                    cur_switch['ports'] = []
                m = hca_re.match(line)
                if m:
                    # Parse HCA description
                    cur_hca = {'name': m.group(3), 'appCount': 1,
                               'guid': m.group(2)}
    return hcas, switches


def parse_ranktohost_csv(filename):
    ranks = []
    with open(filename) as inh:
        inh.readline()
        line = inh.readline()
        while line:
            ranks.append(line.rstrip().split(',')[1])
            line = inh.readline()
    return ranks


def parse_fdbs(filename, switches):
    header_re = re.compile('Switch 0x([0-9A-Fa-f]{16})')
    with open(filename) as inh:
        switch = None
        fdbs = [255]
        for line in inh:
            m = header_re.search(line)
            if m:
                if switch is not None:
                    switch['fdbs'] = fdbs
                for s in switches:
                    if s['guid'] == m.group(1):
                        switch = s
                        break
            elif switch is not None:
                if line.startswith('LID'):
                    continue
                elif line.rstrip() == '':
                    switch['fdbs'] = fdbs
                    switch = None
                else:
                    port = line.split(':')[1].strip()
                    if port == 'UNREACHABLE':
                        fdbs.append(255)
                    else:
                        fdbs.append(int(port))


def output_fdbs(name, switches):
    with open('{}.fdbs'.format(name), 'w') as outh:
        for swid, switch in enumerate(switches):
            outh.write('{}:'.format(swid))
            fdbs = switch['fdbs']
            for x in fdbs:
                outh.write(' {}'.format(x))
            outh.write('\n')


def output_ned(name, ranks, hcas, switches):
    with open('{}.ned'.format(name), 'w') as outh:
        outh.write('package ib_model.networks;\n')
        outh.write('import ib_model.*;\n')
        outh.write('network {}\n'.format(name))
        outh.write('{\n')
        outh.write('\tsubmodules:\n')
        for hca in hcas:
            outh.write('\t\t{}: HCA {{\n'.format(hca['name']))
            outh.write('\t\t\tparameters:\n')
            outh.write('\t\t\t\tsrcLid = {};\n'.format(hca['lid']))
            outh.write('\t\t\t\tappCount = {};\n'.format(hca['appCount']))
            outh.write('\t\t}\n')
        for switch in switches:
            outh.write('\t\t{}: Switch {{\n'.format(switch['name']))
            outh.write('\t\t\tparameters:\n')
            outh.write('\t\t\t\tnumSwitchPorts = {};\n'.format(
                len(switch['ports'])))
            outh.write('\t\t\tgates:\n')
            outh.write('\t\t\t\tport[{}];\n'.format(len(switch['ports'])))
            outh.write('\t\t}\n')
        outh.write('\t\tcontroller: Controller {\n')
        outh.write('\t\t\tgates:\n')
        outh.write('\t\t\t\tout[{}];\n'.format(len(ranks)))
        outh.write('\t\t\t\tdone[{}];\n'.format(len(hcas)))
        outh.write('\t\t}\n')
        outh.write('\tconnections:\n')
        for switch in switches:
            for portid, port in enumerate(switch['ports']):
                outh.write('\t\t{}.port <--> IB{}Wire <--> {}.port[{}];\n'.format(
                    port['remote'], port['speed'], switch['name'], portid))
        for rank, hcaname in enumerate(ranks):
            hca = None
            for h in hcas:
                if h['name'] == hcaname:
                    hca = h
                    break
            if 'appid' not in hca:
                hca['appid'] = 0
            outh.write('\t\tcontroller.out[{}] --> {}.msgIn[{}];\n'.format(
                rank, hcaname, hca['appid']))
            hca['appid'] += 1
        for hcaid, hca in enumerate(hcas):
            outh.write('\t\t{}.msgDone --> controller.done[{}];\n'.format(
                hca['name'], hcaid))
        outh.write('}\n')


def output_ini(name, switches):
    base_dir = '/home/pmacarth/src/omnetpp-workspace/ib_model/utils'
    env = Environment(loader=FileSystemLoader(base_dir))
    template = env.get_template('omnetpp.ini.j2')
    with open('{}.ini'.format(name), 'w') as outf:
        outf.write(template.render(name=name, switches=switches))


def main():
    hcas, switches = parse_ibnetdiscover_output('ibnetdiscover.out')
    ranks = parse_ranktohost_csv('ranktohost.csv')
    parse_fdbs('ibdiagnet.fdbs', switches)
    name = 'test'
    output_ned(name, ranks, hcas, switches)
    output_fdbs(name, switches)
    output_ini(name, switches)

if __name__ == '__main__':
    main()
