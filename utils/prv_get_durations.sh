#!/bin/sh
# utils/prv_get_durations.sh
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

in=$1
out=$(basename $in .prv).csv

printf "\"size_bytes\",\"duration_ns\"\n" >${out}
awk -F: '/^3:/ { printf "%s,%s,%s,%s\n", $14, $2, $8, $13 - $6; }' ${in} >>${out}

# In R:
# df = read.csv('durations.csv')
# aggregate(duration_ns ~ size_bytes, data=df, FUN=mean)
