# Copyright (c) 2026. The SWAT Team. All rights reserved.
#
# This program is free software you can redistribute it and/or modify it
# under the terms of the license (GNU LGPL) which comes with this package.

# End-of-stream: once every publisher has closed its engine, a subscriber asking for a transaction that was never
# produced is released with an EndOfStreamException instead of blocking forever. The outcome (transactions read,
# whether end-of-stream was reached) is recorded in a container shared with the actor and asserted AFTER e.run().
# Asserting inside the subscriber actor would be unsafe: were the mechanism missing, the actor would deadlock,
# e.run() would return with it still blocked, and an in-actor assertion would simply never run (a false pass).

import ctypes
import sys
import multiprocessing
from simgrid import Engine, Host, this_actor, LinkInRoute
from dtlmod import DTL, Engine as DTLEngine, Stream, Transport, EndOfStreamException


def add_cluster(root, suffix, num_hosts):
    cluster = root.add_netzone_star(f"cluster{suffix}")
    cluster.set_gateway(cluster.add_router(f"cluster{suffix}-router"))
    backbone = cluster.add_link(f"backbone{suffix}", "100Gbps").set_latency("100us")
    for h in range(num_hosts):
        host = cluster.add_host(f"host-{h}{suffix}", "1Gf")
        link = cluster.add_link(f"host-{h}{suffix}_link", "10Gbps").set_latency("10us")
        cluster.add_route(host, None, [backbone, link])
    cluster.seal()
    return cluster


def setup_staging_platform():
    e = Engine(sys.argv)
    e.set_log_control("no_loc")
    e.set_log_control("root.thresh:critical")
    root = e.netzone_root
    internet = root.add_link("internet", "500Mbps").set_latency("1ms")
    prod_cluster = add_cluster(root, ".prod", 4)
    cons_cluster = add_cluster(root, ".cons", 4)
    root.add_route(prod_cluster, cons_cluster, [internet])
    root.seal()
    DTL.create()
    return e


def setup_file_platform():
    e = Engine(sys.argv)
    e.set_log_control("no_loc")
    e.set_log_control("root.thresh:critical")
    from fsmod import FileSystem, JBODStorage, OneDiskStorage

    cluster = e.netzone_root.add_netzone_star("cluster")
    pfs_server = cluster.add_host("pfs_server", "1Gf")
    pfs_disks = [pfs_server.add_disk(f"pfs_disk{i}", "2.5GBps", "1.2GBps") for i in range(4)]
    remote_storage = JBODStorage.create("pfs_storage", pfs_disks)
    remote_storage.set_raid_level(JBODStorage.RAID.RAID5)

    local_storages = []
    for i in range(4):
        hostname = f"node-{i}"
        host = cluster.add_host(hostname, "1Gf")
        disk = host.add_disk(f"{hostname}_disk", "5.5GBps", "2.1GBps")
        local_storages.append(OneDiskStorage.create(f"{hostname}_local_storage", disk))
        link_up = cluster.add_link(f"link_{i}_UP", "1Gbps")
        link_down = cluster.add_link(f"link_{i}_DOWN", "1Gbps")
        loopback = cluster.add_link(f"{hostname}_loopback", "10Gbps")
        cluster.add_route(host, None, [LinkInRoute(link_up)], False)
        cluster.add_route(None, host, [LinkInRoute(link_down)], False)
        cluster.add_route(host, host, [loopback])
    cluster.seal()

    my_fs = FileSystem.create("my_fs")
    FileSystem.register_file_system(cluster, my_fs)
    my_fs.mount_partition("/pfs/", remote_storage, "500TB")
    for i in range(4):
        my_fs.mount_partition(f"/node-{i}/scratch/", local_storages[i], "1TB")

    DTL.create()
    return e


def make_publisher(engine_type, method, engine_name, n_tx):
    def pub_actor():
        dtl = DTL.connect()
        stream = dtl.add_stream("my-output")
        stream.set_engine_type(engine_type).set_transport_method(method)
        var = stream.define_variable("var", (100, 100), (0, 0), (100, 100), ctypes.sizeof(ctypes.c_double))
        engine = stream.open(engine_name, Stream.Mode.Publish)
        for _ in range(n_tx):
            engine.begin_transaction()
            engine.put(var)
            engine.end_transaction()
        engine.close()
        DTL.disconnect()
    return pub_actor


def make_subscriber(engine_name, result):
    # result is a shared dict updated in place: {"reads": int, "eos": bool}
    def sub_actor():
        dtl = DTL.connect()
        stream = dtl.add_stream("my-output")
        engine = stream.open(engine_name, Stream.Mode.Subscribe)
        var_sub = stream.inquire_variable("var")
        var_sub.set_selection((0, 0), (100, 100))
        try:
            while True:
                engine.begin_transaction()
                engine.get(var_sub)
                engine.end_transaction()
                result["reads"] += 1
        except EndOfStreamException:
            result["eos"] = True
        engine.close()
        DTL.disconnect()
    return sub_actor


def run_test_staging_single_subscriber_mq():
    e = setup_staging_platform()
    result = {"reads": 0, "eos": False}
    Host.by_name("host-0.prod").add_actor(
        "Pub", make_publisher(DTLEngine.Type.Staging, Transport.Method.MQ, "my-output", 2))
    Host.by_name("host-0.cons").add_actor("Sub", make_subscriber("my-output", result))
    e.run()
    assert result["eos"], "subscriber did not reach end of stream"
    assert result["reads"] == 2, f'read {result["reads"]} transactions, expected 2'


def run_test_staging_single_subscriber_mailbox():
    e = setup_staging_platform()
    result = {"reads": 0, "eos": False}
    Host.by_name("host-0.prod").add_actor(
        "Pub", make_publisher(DTLEngine.Type.Staging, Transport.Method.Mailbox, "my-output", 3))
    Host.by_name("host-0.cons").add_actor("Sub", make_subscriber("my-output", result))
    e.run()
    assert result["eos"], "subscriber did not reach end of stream"
    assert result["reads"] == 3, f'read {result["reads"]} transactions, expected 3'


def run_test_staging_multiple_subscribers_mq():
    e = setup_staging_platform()
    results = [{"reads": 0, "eos": False}, {"reads": 0, "eos": False}]
    Host.by_name("host-0.prod").add_actor(
        "Pub", make_publisher(DTLEngine.Type.Staging, Transport.Method.MQ, "my-output", 2))
    for s in range(2):
        Host.by_name(f"host-{s}.cons").add_actor(f"Sub{s}", make_subscriber("my-output", results[s]))
    e.run()
    for s in range(2):
        assert results[s]["eos"], f"subscriber {s} did not reach end of stream"
        assert results[s]["reads"] == 2, f'subscriber {s} read {results[s]["reads"]} transactions, expected 2'


def run_test_file_engine_single_subscriber():
    e = setup_file_platform()
    result = {"reads": 0, "eos": False}
    engine_name = "cluster:my_fs:/node-0/scratch/my-output"
    Host.by_name("node-0").add_actor(
        "Pub", make_publisher(DTLEngine.Type.File, Transport.Method.File, engine_name, 2))
    Host.by_name("node-1").add_actor("Sub", make_subscriber(engine_name, result))
    e.run()
    assert result["eos"], "subscriber did not reach end of stream"
    assert result["reads"] == 2, f'read {result["reads"]} transactions, expected 2'


if __name__ == '__main__':
    tests = [
        run_test_staging_single_subscriber_mq,
        run_test_staging_single_subscriber_mailbox,
        run_test_staging_multiple_subscribers_mq,
        run_test_file_engine_single_subscriber,
    ]

    all_passed = True
    for test in tests:
        print(f"\nRun {test.__name__} ...")
        p = multiprocessing.Process(target=test)
        p.start()
        p.join()
        if p.exitcode != 0:
            print(f"FAILED: {test.__name__} (exit code {p.exitcode})")
            all_passed = False
        else:
            print(f"PASSED: {test.__name__}")

    if not all_passed:
        sys.exit(1)
