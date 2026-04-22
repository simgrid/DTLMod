# Copyright (c) 2026. The SWAT Team. All rights reserved.
#
# This program is free software you can redistribute it and/or modify it
# under the terms of the license (GNU LGPL) which comes with this package.

import ctypes
import sys
import multiprocessing
from simgrid import Engine, Host, this_actor, LinkInRoute
from dtlmod import DTL, Engine as DTLEngine, Stream, Transport, TransactionCancelledException


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
        link_up   = cluster.add_link(f"link_{i}_UP", "1Gbps")
        link_down = cluster.add_link(f"link_{i}_DOWN", "1Gbps")
        loopback  = cluster.add_link(f"{hostname}_loopback", "10Gbps")
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


# Publisher stuck in begin_transaction() waiting for a subscriber that never shows up.
# Canceller fires at t=0.5s. Publisher catches TransactionCancelledException.
# Subscriber sleeps past the cancel point, then gets it immediately on its begin_transaction().
def run_test_cancel_staging_waiting_for_subscriber_mq():
    e = setup_staging_platform()
    engine_ref = [None]

    def pub_actor():
        dtl    = DTL.connect()
        stream = dtl.add_stream("my-output")
        stream.set_engine_type(DTLEngine.Type.Staging).set_transport_method(Transport.Method.MQ)
        stream.define_variable("var", (1000, 1000), (0, 0), (1000, 1000), ctypes.sizeof(ctypes.c_double))
        engine = stream.open("my-output", Stream.Mode.Publish)
        engine_ref[0] = engine

        Host.by_name("host-1.prod").add_actor("Canceller", canceller_actor)

        this_actor.info("Begin transaction (will block waiting for subscriber)")
        try:
            engine.begin_transaction()
            assert False, "Expected TransactionCancelledException"
        except TransactionCancelledException:
            this_actor.info("Publisher caught TransactionCancelledException as expected")
        DTL.disconnect()

    def sub_actor():
        dtl     = DTL.connect()
        stream  = dtl.add_stream("my-output")
        engine  = stream.open("my-output", Stream.Mode.Subscribe)
        stream.inquire_variable("var")
        this_actor.sleep_for(2.0)
        this_actor.info("Begin transaction (cancelled_ already true)")
        try:
            engine.begin_transaction()
            assert False, "Expected TransactionCancelledException"
        except TransactionCancelledException:
            this_actor.info("Subscriber caught TransactionCancelledException as expected")
        DTL.disconnect()

    def canceller_actor():
        this_actor.sleep_for(0.5)
        this_actor.info("Cancelling the transaction")
        engine_ref[0].cancel_transaction()

    Host.by_name("host-0.prod").add_actor("PubTestActor", pub_actor)
    Host.by_name("host-0.cons").add_actor("SubTestActor", sub_actor)
    e.run()


# Same scenario with Mailbox transport.
def run_test_cancel_staging_waiting_for_subscriber_mailbox():
    e = setup_staging_platform()
    engine_ref = [None]

    def pub_actor():
        dtl    = DTL.connect()
        stream = dtl.add_stream("my-output")
        stream.set_engine_type(DTLEngine.Type.Staging).set_transport_method(Transport.Method.Mailbox)
        stream.define_variable("var", (1000, 1000), (0, 0), (1000, 1000), ctypes.sizeof(ctypes.c_double))
        engine = stream.open("my-output", Stream.Mode.Publish)
        engine_ref[0] = engine

        Host.by_name("host-1.prod").add_actor("Canceller", canceller_actor)

        this_actor.info("Begin transaction (will block waiting for subscriber)")
        try:
            engine.begin_transaction()
            assert False, "Expected TransactionCancelledException"
        except TransactionCancelledException:
            this_actor.info("Publisher caught TransactionCancelledException as expected")
        DTL.disconnect()

    def sub_actor():
        dtl     = DTL.connect()
        stream  = dtl.add_stream("my-output")
        engine  = stream.open("my-output", Stream.Mode.Subscribe)
        stream.inquire_variable("var")
        this_actor.sleep_for(2.0)
        this_actor.info("Begin transaction (cancelled_ already true)")
        try:
            engine.begin_transaction()
            assert False, "Expected TransactionCancelledException"
        except TransactionCancelledException:
            this_actor.info("Subscriber caught TransactionCancelledException as expected")
        DTL.disconnect()

    def canceller_actor():
        this_actor.sleep_for(0.5)
        this_actor.info("Cancelling the transaction")
        engine_ref[0].cancel_transaction()

    Host.by_name("host-0.prod").add_actor("PubTestActor", pub_actor)
    Host.by_name("host-0.cons").add_actor("SubTestActor", sub_actor)
    e.run()


# Subscriber stuck in begin_transaction() waiting for the publisher to start a transaction.
# Publisher opens the stream but sleeps before calling begin_transaction().
# Canceller fires at t=0.5s. Subscriber catches TransactionCancelledException.
def run_test_cancel_staging_waiting_for_publisher_mq():
    e = setup_staging_platform()
    engine_ref = [None]

    def pub_actor():
        dtl    = DTL.connect()
        stream = dtl.add_stream("my-output")
        stream.set_engine_type(DTLEngine.Type.Staging).set_transport_method(Transport.Method.MQ)
        stream.define_variable("var", (1000, 1000), (0, 0), (1000, 1000), ctypes.sizeof(ctypes.c_double))
        engine = stream.open("my-output", Stream.Mode.Publish)
        engine_ref[0] = engine

        Host.by_name("host-1.prod").add_actor("Canceller", canceller_actor)

        this_actor.sleep_for(2.0)
        this_actor.info("Begin transaction (cancelled_ already true)")
        try:
            engine.begin_transaction()
            assert False, "Expected TransactionCancelledException"
        except TransactionCancelledException:
            this_actor.info("Publisher caught TransactionCancelledException as expected")
        DTL.disconnect()

    def sub_actor():
        dtl     = DTL.connect()
        stream  = dtl.add_stream("my-output")
        engine  = stream.open("my-output", Stream.Mode.Subscribe)
        stream.inquire_variable("var")
        this_actor.info("Begin transaction (will block waiting for publisher to start a transaction)")
        try:
            engine.begin_transaction()
            assert False, "Expected TransactionCancelledException"
        except TransactionCancelledException:
            this_actor.info("Subscriber caught TransactionCancelledException as expected")
        DTL.disconnect()

    def canceller_actor():
        this_actor.sleep_for(0.5)
        this_actor.info("Cancelling the transaction")
        engine_ref[0].cancel_transaction()

    Host.by_name("host-0.prod").add_actor("PubTestActor", pub_actor)
    Host.by_name("host-0.cons").add_actor("SubTestActor", sub_actor)
    e.run()


# FileEngine: subscriber stuck in begin_transaction() waiting for publisher to complete a transaction.
# Publisher opens the stream but sleeps before calling begin_transaction().
# Canceller fires at t=0.5s. Subscriber catches TransactionCancelledException.
def run_test_cancel_file_engine_waiting_for_publisher():
    e = setup_file_platform()
    engine_ref = [None]

    def pub_actor():
        dtl    = DTL.connect()
        stream = dtl.add_stream("my-output")
        stream.set_engine_type(DTLEngine.Type.File).set_transport_method(Transport.Method.File)
        stream.define_variable("var", (1000, 1000), (0, 0), (1000, 1000), ctypes.sizeof(ctypes.c_double))
        engine = stream.open("cluster:my_fs:/node-0/scratch/my-output", Stream.Mode.Publish)
        engine_ref[0] = engine

        Host.by_name("node-2").add_actor("Canceller", canceller_actor)

        this_actor.sleep_for(2.0)
        this_actor.info("Begin transaction (cancelled_ already true)")
        try:
            engine.begin_transaction()
            assert False, "Expected TransactionCancelledException"
        except TransactionCancelledException:
            this_actor.info("Publisher caught TransactionCancelledException as expected")
        DTL.disconnect()

    def sub_actor():
        dtl     = DTL.connect()
        stream  = dtl.add_stream("my-output")
        engine  = stream.open("cluster:my_fs:/node-0/scratch/my-output", Stream.Mode.Subscribe)
        stream.inquire_variable("var")
        this_actor.info("Begin transaction (will block waiting for publisher to complete a transaction)")
        try:
            engine.begin_transaction()
            assert False, "Expected TransactionCancelledException"
        except TransactionCancelledException:
            this_actor.info("Subscriber caught TransactionCancelledException as expected")
        DTL.disconnect()

    def canceller_actor():
        this_actor.sleep_for(0.5)
        this_actor.info("Cancelling the transaction")
        engine_ref[0].cancel_transaction()

    Host.by_name("node-0").add_actor("PubTestActor", pub_actor)
    Host.by_name("node-1").add_actor("SubTestActor", sub_actor)
    e.run()


if __name__ == '__main__':
    tests = [
        run_test_cancel_staging_waiting_for_subscriber_mq,
        run_test_cancel_staging_waiting_for_subscriber_mailbox,
        run_test_cancel_staging_waiting_for_publisher_mq,
        run_test_cancel_file_engine_waiting_for_publisher,
    ]

    for test in tests:
        print(f"\nRun {test.__name__} ...")
        p = multiprocessing.Process(target=test)
        p.start()
        p.join()

        if p.exitcode != 0:
            print(f"FAILED: {test.__name__} (exit code {p.exitcode})")
        else:
            print(f"PASSED: {test.__name__}")
