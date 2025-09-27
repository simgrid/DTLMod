# Copyright (c) 2025. The SWAT Team. All rights reserved.
#
# This program is free software you can redistribute it and/or modify it
# under the terms of the license (GNU LGPL) which comes with this package.

import ctypes
import sys
import multiprocessing

from simgrid import Actor, Engine, Host, Link, LinkInRoute, NetZone, this_actor
from dtlmod import DTL, Variable

def setup_platform():
    e = Engine(sys.argv)
    e.set_log_control("no_loc")
    e.set_log_control("root.thresh:critical")

    root = e.netzone_root
    host = root.add_host("host", "1Gf")
    disk = host.add_disk("disk", "1kBps", "2kBps")
    root.seal()

    DTL.create()
    return e, host

def run_test_define_variable():
    e, host = setup_platform()

    def define_variable():
        this_actor.info("Connect to the DTL")
        dtl = DTL.connect()
        this_actor.info("Create a stream")
        stream = dtl.add_stream("Stream")
        this_actor.info("Create a scalar int variable");
        var = stream.define_variable("scalar", ctypes.sizeof(ctypes.c_int))
        this_actor.info("Create a 3D variable")
        var3D = stream.define_variable("var3D", (64, 64, 64), (0, 0, 0), (64, 64, 64), 
                                       ctypes.sizeof(ctypes.c_double))
        this_actor.info("Check name")
        assert var3D.name == "var3D"
        this_actor.info("Check size: should be 64^3 times 8 as elements are double")
        assert var3D.get_global_size() == 64 * 64 * 64 * 8
        this_actor.info("Remove variable named 'var3D'. It is known, should be true")
        assert stream.remove_variable("var3D") == True
        this_actor.info("Remove variable named 'var2D'. It is unknown, should be false");
        assert stream.remove_variable("var2D") == False

        this_actor.info("Disconnect from the DTL")
        DTL.disconnect()

        host.add_actor("client", define_variable)
        e.run()

if __name__ == '__main__':
    tests = [
        run_test_define_variable
    ]

    for test in tests:
        print(f"\n🔧 Run {test.__name__} ...")
        p = multiprocessing.Process(target=test)
        p.start()
        p.join()

        if p.exitcode != 0:
            print(f"❌ {test.__name__} failed with exit code {p.exitcode}")
        else:
            print(f"✅ {test.__name__} passed")
