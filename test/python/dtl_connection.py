# Copyright (c) 2025. The SWAT Team. All rights reserved.
#
# This program is free software you can redistribute it and/or modify it
# under the terms of the license (GNU LGPL) which comes with this package.

import sys
import multiprocessing
from simgrid import Actor, Engine, Host, Link, LinkInRoute, NetZone, this_actor
from dtlmod import DTL

def setup_platform():
    e = Engine(sys.argv)
    e.set_log_control("no_loc")
    e.set_log_control("root.thresh:critical")

    cluster = e.netzone_root.add_netzone_star("cluster")
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

    DTL.create()
    return e, e.all_hosts

def run_test_sync_con_sync_decon():
    e, hosts = setup_platform()

    def sync_con_sync_decon():
        this_actor.info("Connect to the DTL")
        dtl = DTL.connect()
        this_actor.sleep_for(1)
        this_actor.info("Disconnect from the DTL")
        DTL.disconnect()

    for i in range(4):
        hosts[i].add_actor(f"client-{i}", sync_con_sync_decon)

    e.run()
    
def run_test_async_con_sync_decon():
    e, hosts = setup_platform()

    def async_con_sync_decon(idx):
        this_actor.sleep_for(0.1 * idx)
        this_actor.info("Connect to the DTL")
        dtl = DTL.connect()
        this_actor.sleep_for(1 - (0.1 * idx))
        this_actor.info("Disconnect from the DTL")
        DTL.disconnect()

    for i in range(4):
        hosts[i].add_actor(f"client-{i}", async_con_sync_decon, i)

    e.run()
    
def run_test_sync_con_async_decon():
    e, hosts = setup_platform()

    def sync_con_async_decon(idx):
        dtl = DTL.connect()
        this_actor.sleep_for(1 - (0.1 * idx))
        this_actor.sleep_for(1)
        DTL.disconnect()

    for i in range(4):
        hosts[i].add_actor(f"client-{i}", sync_con_async_decon, i)

    e.run()
    
def run_test_async_con_async_decon():
    e, hosts = setup_platform()

    def async_con_async_decon(idx):
        this_actor.sleep_for(0.1 * idx)
        dtl = DTL.connect()
        this_actor.sleep_for(1)
        DTL.disconnect()

    for i in range(4):
        hosts[i].add_actor(f"client-{i}", async_con_async_decon, i)

    e.run()
    
if __name__ == '__main__':
    tests = [
        run_test_sync_con_sync_decon,
        run_test_async_con_sync_decon,
        run_test_sync_con_async_decon,
        run_test_async_con_async_decon
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
