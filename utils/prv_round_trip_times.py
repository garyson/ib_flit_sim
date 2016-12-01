#!/usr/bin/env python3
# utils/prv_round_trip_times.py
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

# Usage: prvresync.py input_trace.prv CPU:offset

import os.path
import re
import sys

_recordTypes = dict()

class ParaverNode:
    def __init__(self, fields):
        self.cpu_id = int(fields.pop(0))
        self.appl_id = int(fields.pop(0))
        self.task_id = int(fields.pop(0))
        self.thread_id = int(fields.pop(0))

    def __str__(self):
        return "{}:{}:{}:{}".format(str(self.cpu_id), str(self.appl_id),
                str(self.task_id), str(self.thread_id))

    def __eq__(self, other):
        for member in ['cpu_id', 'appl_id', 'task_id', 'thread_id']:
            if getattr(self, member) != getattr(other, member):
                return False
        return True

class ParaverRecord:
    def __gt__(self, other):
        if self.sort_time > other.sort_time:
            return True
        elif self.sort_time == other.sort_time:
            if self.record_type < other.record_type:
                return True
        return False

    def __lt__(self, other):
        if self.sort_time < other.sort_time:
            return True
        elif self.sort_time == other.sort_time:
            if self.record_type > other.record_type:
                return True
        return False

    def __eq__(self, other):
        return self.sort_time == other.sort_time \
                and self.record_type == other.record_type

    @classmethod
    def parse(cls, line):
        fields = line.split(':')
        rtype = int(fields.pop(0))
        return _recordTypes[rtype](fields)

class ParaverStateRecord(ParaverRecord):
    def __init__(self, fields):
        self.node = ParaverNode(fields[0:4])
        self.begin_time = int(fields[4])
        self.end_time = int(fields[5])
        self.state_id = int(fields[6])

    def __str__(self):
        return "1:{}:{}:{}:{}".format(self.node, self.begin_time,
                self.end_time, self.state_id)

    @property
    def record_type(self):
        return 1

    @property
    def sort_time(self):
        return self.begin_time

    def adjust_time_for_cpu(self, cpu_id, offset):
        if self.node.cpu_id == cpu_id:
            if self.begin_time != 0:
                self.begin_time += offset
            self.end_time += offset

_recordTypes[1] = ParaverStateRecord

class ParaverEventRecord(ParaverRecord):
    class Event:
        def __init__(self, etype, value):
            self.event_type = int(etype)
            self.event_value = int(value)

        def __str__(self):
            return "{}:{}".format(self.event_type, self.event_value)

    def __init__(self, fields):
        self.node = ParaverNode(fields[0:4])
        self.time = int(fields[4])
        fields = fields[5:]
        self.events = []
        while len(fields) != 0:
            event_type = fields.pop(0)
            event_value = fields.pop(0)
            self.events.append(type(self).Event(event_type, event_value))

    def __str__(self):
        s = "2:{}:{}".format(self.node, self.time)
        for e in self.events:
            s += ":" + str(e)
        return s

    @property
    def record_type(self):
        return 2

    @property
    def sort_time(self):
        return self.time

    def adjust_time_for_cpu(self, cpu_id, offset):
        if self.node.cpu_id == cpu_id and self.time != 0:
            self.time += offset

_recordTypes[2] = ParaverEventRecord

class ParaverCommunicationRecord(ParaverRecord):
    def __init__(self, fields):
        self.sender = ParaverNode(fields[0:4])
        self.lsend = int(fields[4])
        self.psend = int(fields[5])
        self.recver = ParaverNode(fields[6:10])
        self.precv = int(fields[10])
        self.lrecv = int(fields[11])
        self.size = int(fields[12])
        self.tag = int(fields[13])

    def __str__(self):
        return "3:{}:{}:{}:{}:{}:{}:{}:{}".format(
                self.sender, self.lsend, self.psend, self.recver,
                self.precv, self.lrecv, self.size, self.tag)

    @property
    def record_type(self):
        return 3

    @property
    def sort_time(self):
        return self.lsend

    def adjust_time_for_cpu(self, cpu_id, offset):
        if self.sender.cpu_id == cpu_id:
            if self.lsend != 0:
                self.lsend += offset
            if self.psend != 0:
                self.psend += offset
        if self.recver.cpu_id == cpu_id:
            if self.precv != 0:
                self.precv += offset
            if self.lrecv != 0:
                self.lrecv += offset

_recordTypes[3] = ParaverCommunicationRecord

def main():
    in_fn = sys.argv[1]
    out_fn = os.path.splitext(in_fn)[0] + '.rtt.csv'
    last_lsend = dict()
    with open(in_fn, 'r') as in_fh, open(out_fn, 'w') as out_fh:
        print('size_bytes', 'sender', 'receiver', 'duration_ns', sep=',', file=out_fh)
        for line in in_fh:
            if line[0] != '3':
                continue

            record = ParaverRecord.parse(line)
            send_key = (record.sender.cpu_id, record.recver.cpu_id)
            recv_key = (record.recver.cpu_id, record.sender.cpu_id)
            if recv_key in last_lsend:
                print(record.size, record.recver.cpu_id, record.sender.cpu_id,
                    record.lrecv - last_lsend[recv_key], sep=',', file=out_fh)
            last_lsend[send_key] = record.lsend

if __name__ == '__main__':
    main()
