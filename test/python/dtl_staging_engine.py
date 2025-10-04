# Copyright (c) 2025. The SWAT Team. All rights reserved.
#
# This program is free software you can redistribute it and/or modify it
# under the terms of the license (GNU LGPL) which comes with this package.

import ctypes
import sys
import multiprocessing
from simgrid import Engine, Host, this_actor
from dtlmod import DTL, Engine as DTLEngine, Stream, Transport

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

def setup_platform():
    e = Engine(sys.argv)
    e.set_log_control("no_loc")
    e.set_log_control("root.thresh:critical")

    root = e.netzone_root
    internet = root.add_link("internet", "500Mbps").set_latency("1ms")
    prod_cluster = add_cluster(root, ".prod", 16)
    cons_cluster = add_cluster(root, ".cons", 4)
    root.add_route(prod_cluster, cons_cluster, [internet])
    root.seal()

    DTL.create()
    return e

def run_test_single_pub_single_sub_same_cluster():
    e = setup_platform()

    def pub_test_actor():
        dtl = DTL.connect()
        stream = dtl.add_stream("my-output").set_engine_type(DTLEngine.Type.Staging).set_transport_method(Transport.Method.MQ)
        this_actor.info("Create a 2D-array variable with 20kx20k double")
        var = stream.define_variable("var", (20000, 20000), (0, 0), (20000, 20000), ctypes.sizeof(ctypes.c_double))
        this_actor.info("Open the stream")
        engine = stream.open("my-output", Stream.Mode.Publish)
        this_actor.info(f"Stream {stream.name} is opened")
        this_actor.sleep_for(1)

        this_actor.info("Start a transaction")
        engine.begin_transaction()
        this_actor.info("Put Variable 'var' into the DTL")
        engine.put(var, var.local_size)
        this_actor.info("End the transaction")
        engine.end_transaction()

        this_actor.info("Close the engine")
        engine.close()
        this_actor.info("Disconnect from the DTL")
        DTL.disconnect()

    def sub_test_actor():
        dtl = DTL.connect()
        stream = dtl.add_stream("my-output")
        engine = stream.open("my-output", Stream.Mode.Subscribe)
        var_sub = stream.inquire_variable("var")
        assert var_sub.name == "var"
        shape = var_sub.shape
        assert shape[0] == 20000 and shape[1] == 20000
        assert var_sub.global_size == 20000 * 20000 * ctypes.sizeof(ctypes.c_double)

        this_actor.info("Set a selection for 'var_sub': Just get the second half of the first dimension")
        var_sub.set_selection((10000, 0), (10000, shape[1]))

        this_actor.info("Start a transaction")
        engine.begin_transaction()
        this_actor.info("Get a subset of the Variable 'var' from the DTL")
        engine.get(var_sub)
        this_actor.info("End the transaction")
        engine.end_transaction()

        this_actor.info("Check local size of var_sub. Should be 1,600,000,000 bytes")
        assert var_sub.local_size == 8 * 10000 * 20000

        this_actor.info("Close the engine")
        engine.close()
        this_actor.info("Disconnect from the DTL")
        DTL.disconnect()

    Host.by_name("host-0.prod").add_actor("PubTestActor", pub_test_actor)
    Host.by_name("host-0.cons").add_actor("SubTestActor", sub_test_actor)
    
    e.run()

def run_test_multiple_pub_single_sub_message_queue():
    e = setup_platform()

    def pub_test_actor(id):
        dtl = DTL.connect()
        stream = dtl.add_stream("my-output").set_engine_type(DTLEngine.Type.Staging).set_transport_method(Transport.Method.MQ)
        this_actor.info("Create a 2D-array variable with 10kx10k double, publishers own 3/4 and 1/4 (along 2nd dimension)")
        var = stream.define_variable("var", (10000, 10000), (0, 2500 * 3 * id), (10000, 7500 - (5000 * id)), ctypes.sizeof(ctypes.c_double))
        engine = stream.open("my-output", Stream.Mode.Publish)
        this_actor.info("Wait for all publishers to have opened the stream")
        this_actor.sleep_for(.5)

        this_actor.info("Start a transaction")
        engine.begin_transaction()
        engine.put(var, var.local_size)
        this_actor.info("End the transaction")
        engine.end_transaction()

        this_actor.sleep_for(.5)
        this_actor.info("Close the engine")
        engine.close()
        this_actor.info("Disconnect from the DTL")
        DTL.disconnect()

    def sub_test_actor(id):
        dtl = DTL.connect()
        stream = dtl.add_stream("my-output")
        engine = stream.open("my-output", Stream.Mode.Subscribe)
        var_sub = stream.inquire_variable("var")
        

        this_actor.info("Set a selection for 'var_sub': get 3/4 and 1/4 of the first dimension respectively")
        var_sub.set_selection((2500 * 3 * id , 0), (7500 - (5000 * id), 10000))

        this_actor.info("Start a transaction")
        engine.begin_transaction()
        this_actor.info("Get a subset of the Variable 'var' from the DTL")
        engine.get(var_sub)
        this_actor.info("End the transaction")
        engine.end_transaction()

        this_actor.info("Check local size of var_sub")
        assert var_sub.local_size == 8 * 10000 * 10000 * (3 - 2 * id) / 4
        this_actor.info("Close the engine")
        engine.close()
        this_actor.info("Disconnect from the DTL")
        DTL.disconnect()
    
    for i in range(2):
        Host.by_name(f"host-{i}.prod").add_actor(f"PubTestActor{i}", pub_test_actor, i)
        Host.by_name(f"host-{i}.cons").add_actor(f"SubTestActor{i}", sub_test_actor, i)

    e.run()

def run_test_multiple_pub_single_sub_mailbox():
    e = setup_platform()

    def pub_test_actor(id):
        dtl = DTL.connect()
        stream = dtl.add_stream("my-output").set_engine_type(DTLEngine.Type.Staging).set_transport_method(Transport.Method.Mailbox)
        this_actor.info("Create a 2D-array variable with 10kx10k double, publishers own 3/4 and 1/4 (along 2nd dimension)")
        var = stream.define_variable("var", (10000, 10000), (0, 2500 * 3 * id), (10000, 7500 - (5000 * id)), ctypes.sizeof(ctypes.c_double))
        engine = stream.open("my-output", Stream.Mode.Publish)
        this_actor.info("Wait for all publishers to have opened the stream")
        this_actor.sleep_for(.5)

        this_actor.info("Start a transaction")
        engine.begin_transaction()
        engine.put(var, var.local_size)
        this_actor.info("End the transaction")
        engine.end_transaction()

        this_actor.sleep_for(.5)
        this_actor.info("Close the engine")
        engine.close()
        this_actor.info("Disconnect from the DTL")
        DTL.disconnect()

    def sub_test_actor(id):
        dtl = DTL.connect()
        stream = dtl.add_stream("my-output")
        engine = stream.open("my-output", Stream.Mode.Subscribe)
        var_sub = stream.inquire_variable("var")
        

        this_actor.info("Set a selection for 'var_sub': get 3/4 and 1/4 of the first dimension respectively")
        var_sub.set_selection((2500 * 3 * id , 0), (7500 - (5000 * id), 10000))

        this_actor.info("Start a transaction")
        engine.begin_transaction()
        this_actor.info("Get a subset of the Variable 'var' from the DTL")
        engine.get(var_sub)
        this_actor.info("End the transaction")
        engine.end_transaction()

        this_actor.info("Check local size of var_sub")
        assert var_sub.local_size == 8 * 10000 * 10000 * (3 - 2 * id) / 4
        this_actor.info("Close the engine")
        engine.close()
        this_actor.info("Disconnect from the DTL")
        DTL.disconnect()
    
    for i in range(2):
        Host.by_name(f"host-{i}.prod").add_actor(f"PubTestActor{i}", pub_test_actor, i)
        Host.by_name(f"host-{i}.cons").add_actor(f"SubTestActor{i}", sub_test_actor, i)

    e.run()

if __name__ == '__main__':
    tests = [
        run_test_single_pub_single_sub_same_cluster,
        run_test_multiple_pub_single_sub_message_queue,
        run_test_multiple_pub_single_sub_mailbox
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
