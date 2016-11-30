Infiniband Flit Simulation
==========================

This directory tree hold an InfiniBand model with support for
IB flow control scheme, Arbitration over multiple VLs and routing
based on Linear Forwarding Tables.

This model is derived and significantly enhanced from a [model][1]
originally developed by Mellanox and released under the GPLv2 license.

The most significant enchancement over the original model is support
for trace-based simulation of MPI applications via integration with the
Dimemas simulator from the Barcelona Supercomputing Center.

To run it:
1. make makefiles
2. make

This will make a debug build by default.  To create an optimized build,
add `MODE=release` like this:

    $ make MODE=release

By default, this will build a shared library which is needed by the
testsuite.  To build an executable instead, you need to rebuild the
makefiles:

    $ make makefiles-exe

To run the example models, cd into the example dir and look for a README

For more info regarding each one of the model modules see the doc directory

[1]: https://web.archive.org/web/20130811125321/http://www.mellanox.com/page/omnet
