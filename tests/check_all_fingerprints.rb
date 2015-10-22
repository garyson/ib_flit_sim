#!/usr/bin/env ruby
# check_all_fingerprints.rb

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
