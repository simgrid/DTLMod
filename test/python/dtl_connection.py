# Copyright (c) 2025. The SWAT Team. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the license (GNU LGPL) which comes with this package.

import sys
from simgrid import Actor, Engine, Host, this_actor
from dtlmod import DTL

class testActor:
    """
    Test actor: connect to/disconnect from the DTL
    """

    def __call__(self):
        this_actor.sleep_for(1)
        dtl = DTL.connect()
        this_actor.sleep_for(1)
        DTL.disconnect()

def load_platform(e: Engine):
    """ Creates a mixed platform, using many methods available in the API
    """

    root = e.netzone_root.add_netzone_full("root")
    host = root.add_host("host", "1Gf")
    host.add_disk("disk", "1kBps", "2kBps")
    root.seal()

    # create the DTL
    DTL.create()

    # create actor
    host.add_actor("testAtcor", testActor())

###################################################################################################

if __name__ == '__main__':
    e = Engine(sys.argv)

    # create platform
    load_platform(e)

    # runs the simulation
    e.run()
