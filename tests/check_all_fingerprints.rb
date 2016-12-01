#!/usr/bin/env ruby
# tests/check_all_fingerprints.rb
#
# InfiniBand FLIT (Credit) Level OMNet++ Simulation Model
#
# Copyright (c) 2015-2016 University of New Hampshire InterOperability Laboratory
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

# Author:: Patrick MacArthur <pmacarth@iol.unh.edu>

require 'yaml'

NED_PATH = '..:../../src'
TESTSDIR = "#{File.dirname(Process.argv0)}"
TOPDIR = File.realpath("#{TESTSDIR}/..")

yamldata = File.open("#{TESTSDIR}/fingerprints.yml") do |f|
  f.read(nil)
end
tests = YAML.load(yamldata)

pass_count = 0
tests.each do |test|
  Dir.chdir("#{TOPDIR}/#{test['workdir']}")
  args = ['opp_run', '-l', '../../src/libib_flit_sim.so', '-n', NED_PATH]
  args << '-u' << 'Cmdenv'
  if test.has_key?('config') then
    args << '-c' << test['config']
  end
  args << "--fingerprint=#{test['fingerprint']}"
  args << test['inifile']
  puts "Running #{test['workdir']}/#{test['inifile']}"
  IO.popen(args) do |io|
    io.each_line do |line|
      if line =~ /Fingerprint successfully verified/ then
        puts line
        pass_count += 1
      else
        match = /Fingerprint mismatch! calculated: (\h{4}-\h{4})/.match(line)
        if match then
          puts line
          test['fingerprint'] = match[1]
        end
      end
    end
  end
end

puts YAML.dump(tests)
puts "Passed: #{pass_count}/#{tests.count}"
