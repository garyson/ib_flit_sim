#!/usr/bin/env python3
#
# Userspace Software iWARP library for DPDK
#
# Authors: Patrick MacArthur <pmacarth@iol.unh.edu>
#
# Copyright (c) 2016, IBM Corporation
# Copyright (c) 2016, University of New Hampshire InterOperability Laboratory
#
# This software is available to you under a choice of one of two
# licenses.  You may choose to be licensed under the terms of the GNU
# General Public License (GPL) Version 2, available from the file
# COPYING in the main directory of this source tree, or the
# BSD license below:
#
#   Redistribution and use in source and binary forms, with or
#   without modification, are permitted provided that the following
#   conditions are met:
#
#   - Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#
#   - Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
#   - Neither the name of IBM nor the names of its contributors may be
#     used to endorse or promote products derived from this software without
#     specific prior written permission.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
# BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
# ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

"""Creates a plot of message durations."""

import os.path
import sys
import yaml

import pandas as pd
import matplotlib.pyplot as plt
from scipy.stats import trim_mean

col_duration = 'duration_ns'
col_size = 'size_bytes'

def load_data_frame(inf):
    """Creates a data frame from the perftest output."""
    fulldata = pd.read_csv(inf)
    filtered = fulldata[fulldata[col_size] <= 131072]
    filtered = filtered[filtered[col_size] > 0]

    print(inf + ':')
    grouped = filtered.groupby(col_size, as_index=False)
    print(grouped.describe())
    #means = grouped.min()
    means = grouped.aggregate(lambda x : trim_mean(x, .05))
    errs = grouped.sem()
    print(grouped.quantile(0.75)[col_duration] -
            grouped.quantile(0.25)[col_duration])
    return means, errs

def size_formatter(x, pos):
    """Return a human-readable string for the given size.

    The size is input in bytes and output in either bytes or some larger
    unit.

    >>> size_formatter(50000000000)
    '5 GB'

    """
    x = int(x)
    if x < 1000:
        return '{:d}'.format(x)
    elif x < 1000000:
        return '{} kB'.format(x // 1000)
    elif x < 1000000000:
        return '{} MB'.format(x // 1000000)
    else:
        return '{} GB'.format(x // 1000000000)


def main():
    """Run perftest benchmarks and generates plots."""
    do_run_test = False
    if len(sys.argv) < 2:
        config_fn = 'config.yml'
    else:
        config_fn = sys.argv[1]
    inf = open(config_fn)
    try:
        config = yaml.load(inf)['config']
    finally:
        inf.close()

    fig = plt.figure()
    ax = fig.add_subplot(1, 1, 1)
    files = {'{}.rtt.csv'.format(os.path.splitext(config['trace_file'])[0]): 'bd:',
             'predicted-{}.rtt.csv'.format(config['name']): 'ro-'}
    for app, fmt in files.items():
        nextdf, errdf = load_data_frame(app)
        errs = errdf[col_duration]
        plt.errorbar(nextdf[col_size], nextdf[col_duration], fmt=fmt, axes=ax,
                yerr=errs, label=app)
    ax.set_xlabel('Message size (bytes)')
    if config['plot']['log_x']:
        ax.set_xscale("log", basex=2, nonposx='clip')
    if config['plot']['log_y']:
        ax.set_xscale("log", basex=2, nonposx='clip')
    #ax.set_xlim(0.5, 200000)
    ax.set_ylabel('Round trip time (nanoseconds)')
    ax.set_title('osu_latency')
    ax.legend(files, loc=0)
    ax.margins(x=0.05, y=0.05)
    plt.savefig('delay.pdf')

if __name__ == '__main__':
    main()
