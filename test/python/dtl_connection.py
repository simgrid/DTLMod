# Copyright (c) 2025. The SWAT Team. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the license (GNU LGPL) which comes with this package.

import sys, unittest
from simgrid import Actor, Engine, Host, Link, LinkInRoute, NetZone, this_actor
from dtlmod import DTL

class DTLConnectionTest(unittest.TestCase):
    e = Engine(sys.argv)
    e.set_log_control("no_loc")
    e.set_log_control("root.thresh:critical")
    print(f"[==========] Running 4 tests from DTLConnectionTest")

    hosts_ = []

    def setup_platform(self):
        cluster = self.e.netzone_root.add_netzone_star("cluster")
        for i in range(4):
            hostname = f"node-{i}"

            host = cluster.add_host(hostname, "1Gf")

            linkname = f"cluster_link_{i}"
            link_up = cluster.add_link(linkname + "_UP", "1Gbps")
            link_down = cluster.add_link(linkname + "_DOWN", "1Gbps")
            loopback = cluster.add_link(hostname + "_loopback", "10Gbps").set_sharing_policy(Link.SharingPolicy.FATPIPE, None)

            cluster.add_route(host, None, [LinkInRoute(link_up)], False)
            cluster.add_route(None, host, [LinkInRoute(link_down)], False)
            cluster.add_route(host, host, [loopback])
        cluster.seal()

        self.hosts_ = self.e.all_hosts

        # Create the DTL
        DTL.create()

    def test_sync_con_sync_decon(self):
        print(f"[ RUN      ] DTLConnectionTest.SyncConSyncDecon")
        self.setup_platform()

        def sync_con_sync_decon():
            this_actor.info("Connect to the DTL")
            dtl = DTL.connect()
            this_actor.info("Let the actor sleep for 1 second")
            this_actor.sleep_for(1)
            this_actor.info("Disconnect the actor from the DTL")
            DTL.disconnect()

        for i in range(4):
            self.hosts_[i].add_actor(f"client-{i}", sync_con_sync_decon)

        self.e.run()
        print(f"[      OK ] DTLConnectionTest.SyncConSyncDecon")

    def test_async_con_sync_decon(self):
        print(f"[ RUN      ] DTLConnectionTest.AsyncConSyncDecon")
        self.setup_platform()

        def async_con_sync_decon(idx):
            this_actor.info(f"Let actor {this_actor.get_name()} sleep for {0.1 * idx} second")
            this_actor.sleep_for(0.1 * idx)
            this_actor.info("Connect to the DTL")
            dtl = DTL.connect()
            this_actor.info(f"Let actor {this_actor.get_name()} sleep for {1 - (0.1 * idx)} second")
            this_actor.sleep_for(1 - (0.1 * idx))
            this_actor.info("Disconnect the actor from the DTL")
            DTL.disconnect()

        for i in range(4):
            self.hosts_[i].add_actor(f"client-{i}", async_con_sync_decon, i)

        self.e.run()
        print(f"[      OK ] DTLConnectionTest.AsyncConSyncDecon")

    def test_sync_con_async_decon(self):
        print(f"[ RUN      ] DTLConnectionTest.SyncConAsyncDecon")
        self.setup_platform()

        def sync_con_async_decon(idx):
            this_actor.info("Connect to the DTL")
            dtl = DTL.connect()
            this_actor.info(f"Let actor {this_actor.get_name()} sleep for {1 - (0.1 * idx)} second")
            this_actor.sleep_for(1 - (0.1 * idx))
            this_actor.sleep_for(1)
            this_actor.info("Disconnect the actor from the DTL")
            DTL.disconnect()

        for i in range(4):
            self.hosts_[i].add_actor(f"client-{i}", sync_con_async_decon, i)

        self.e.run()
        print(f"[      OK ] DTLConnectionTest.SyncConAsyncDecon")

    def test_async_con_async_decon(self):
        print(f"[ RUN      ] DTLConnectionTest.AsyncConAsyncDecon")
        self.setup_platform()

        def async_con_async_decon(idx):
            this_actor.info(f"Let actor {this_actor.get_name()} sleep for {0.1 * idx} second")
            this_actor.sleep_for(0.1 * idx)
            this_actor.info("Connect to the DTL")
            dtl = DTL.connect()
            this_actor.info(f"Let actor {this_actor.get_name()} sleep for 1 second")
            this_actor.sleep_for(1)
            this_actor.info("Disconnect the actor from the DTL")
            DTL.disconnect()

        for i in range(4):
            self.hosts_[i].add_actor(f"client-{i}", async_con_async_decon, i)

        self.e.run()
        print(f"[      OK ] DTLConnectionTest.AsyncConAsyncDecon")

###################################################################################################

if __name__ == '__main__':
    unittest.main()
