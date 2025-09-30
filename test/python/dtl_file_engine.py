# Copyright (c) 2025. The SWAT Team. All rights reserved.
#
# This program is free software you can redistribute it and/or modify it
# under the terms of the license (GNU LGPL) which comes with this package.

import ctypes
import sys
import multiprocessing
from simgrid import Engine, Host, this_actor, Link, LinkInRoute
from fsmod import FileSystem, OneDiskStorage, JBODStorage
from dtlmod import DTL, Engine as DTLEngine, Stream, Transport

def add_cluster(root, suffix, num_hosts):
    
    return cluster  

def setup_platform():
    e = Engine(sys.argv)
    e.set_log_control("no_loc")
    e.set_log_control("root.thresh:critical")
    
    cluster = e.netzone_root.add_netzone_star("cluster")
    pfs_server = cluster.add_host("pfs_server", "1Gf")
    pfs_disks = []
    for i in range(4):
        pfs_disks.append(pfs_server.add_disk(f"pfs_disk_{i}", "2.5GBps", "1.2GBps"))
    remote_storage = JBODStorage.create("pfs_storage", pfs_disks)
    remote_storage.set_raid_level(JBODStorage.RAID.RAID5)

    local_storages = []
    for h in range(4):
        host = cluster.add_host(f"node-{h}", "1Gf")
        disk = host.add_disk(f"node-{h}_disk", "5.5GBps", "2.1GBps")
        local_storages.append(OneDiskStorage.create(f"node-{h}_local_storage", disk))
        
        link_up = cluster.add_link(f"link-{h}_UP", "1Gbps")
        link_down = cluster.add_link(f"link-{h}_DOWN", "1Gbps")
        loopback = cluster.add_link(f"host-{h}_loopback", "10Gbps").set_sharing_policy(Link.SharingPolicy.FATPIPE, None)
        cluster.add_route(host, None, [LinkInRoute(link_up)], False)
        cluster.add_route(None, host, [LinkInRoute(link_down)], False)
        cluster.add_route(host, host, [loopback])
    cluster.seal()

    # Create a file system with multiple partitions: 1 per host and 1 Parallel file System
    my_fs = FileSystem.create("my_fs")
    FileSystem.register_file_system(cluster, my_fs)
    my_fs.mount_partition("/pfs/", remote_storage, "500TB")
    for h in range(4):
        my_fs.mount_partition(f"/node-{h}/scratch/", local_storages[h], "1TB")

    DTL.create()
    return e

def run_test_single_pub_local_storage():
    e = setup_platform()

    def test_actor():
        dtl = DTL.connect()
        stream = dtl.add_stream("my-output").set_engine_type(DTLEngine.Type.File).set_transport_method(Transport.Method.File)
        this_actor.info("Create a 2D-array variable with 20kx20k double")
        var = stream.define_variable("var", (20000, 20000), (0, 0), (20000, 20000), ctypes.sizeof(ctypes.c_double))
        this_actor.info("Open the stream")
        engine = stream.open("cluster:my_fs:/node-0/scratch/my-working-dir/my-output", Stream.Mode.Publish)
        this_actor.info(f"Stream {stream.name} is ready for Publish data into the DTL")
        for i in range(5):
            this_actor.sleep_for(1)
            this_actor.info("Start a transaction")
            engine.begin_transaction()
            this_actor.info("Put Variable 'var' into the DTL")
            engine.put(var, var.local_size)
            this_actor.info("End the transaction")
            engine.end_transaction()

        this_actor.info("Close the engine")
        engine.close()
        
        file_system = FileSystem.file_systems_by_netzone(e.netzone_by_name("cluster"))["my_fs"]
        dirname = "/node-0/scratch/my-working-dir/my-output"
        file_list = file_system.files_in_directory(dirname)
        this_actor.info(f"List contents of {dirname}")
        for filename in file_list:
            this_actor.info(f" - {filename} of size {file_system.file_size(f"{dirname}/{filename}")} bytes")
        this_actor.info("Check the size of the produced file. Should be 5*(20k*20k*8)")
        assert file_system.file_size(f"{dirname}/data.0") == 5 * (20000 * 20000 * 8)
        
        this_actor.info("Disconnect from the DTL")
        DTL.disconnect()

    Host.by_name("node-0").add_actor("TestActor", test_actor)
    
    e.run()

def run_test_single_pub_single_sub_local_storage():
    e = setup_platform()

    def test_actor():
        dtl = DTL.connect()
        stream = dtl.add_stream("my-output").set_engine_type(DTLEngine.Type.File).set_transport_method(Transport.Method.File)
        this_actor.info("Create a 2D-array variable with 20kx20k double")
        var = stream.define_variable("var", (20000, 20000), (0, 0), (20000, 20000), ctypes.sizeof(ctypes.c_double))
        this_actor.info("Open the stream")
        engine = stream.open("cluster:my_fs:/node-0/scratch/my-working-dir/my-output", Stream.Mode.Publish)
        this_actor.info(f"Stream {stream.name} is ready for Publish data into the DTL")
        
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

        this_actor.info("Check that no Actor is connected to the DTL anymore")
        assert dtl.has_active_connections == False
        this_actor.info("Wait until 10s before becoming a Subscriber")
        this_actor.sleep_for(10)
        dtl = DTL.connect()
        engine = stream.open("cluster:my_fs:/node-0/scratch/my-working-dir/my-output", Stream.Mode.Subscribe)
        var_sub = stream.inquire_variable("var")
        shape = var_sub.shape
        assert var_sub.name == "var"
        assert var_sub.global_size == 8. * 20000 * 20000

        this_actor.info("Start a transaction")
        engine.begin_transaction()
        this_actor.info("Get the entire Variable 'var' from the DTL")
        engine.get(var_sub)
        this_actor.info("End the transaction")
        engine.end_transaction()

        this_actor.info("Set a selection for 'var_sub': Just get the second half of the first dimension")
        var_sub.set_selection((10000, 0), (10000, shape[1]))

        this_actor.info("Start a transaction")
        engine.begin_transaction()
        this_actor.info("Get a subset of the Variable 'var' from the DTL")
        engine.get(var_sub)
        this_actor.info("End the transaction")
        engine.end_transaction()
        this_actor.info("Check local size of var_sub. Should be 1,600,000,000 bytes")
        assert var_sub.local_size == 8. * 10000 * 20000

        this_actor.info("Close the engine")
        engine.close()
        this_actor.info("Disconnect from the DTL")
        DTL.disconnect()

    Host.by_name("node-0").add_actor("TestActor", test_actor)
    
    e.run()

def run_test_multiple_pub_single_sub_shared_storage():
    e = setup_platform()
    pub_hosts = [Host.by_name("node-0"), Host.by_name("node-1")]
    sub_host  = Host.by_name("node-2")

    def pub_test_actor(id):
        dtl = DTL.connect()
        stream = dtl.add_stream("my-output").set_engine_type(DTLEngine.Type.File).set_transport_method(Transport.Method.File)
        this_actor.info("Create a 2D-array variable with 20kx20k double, each publisher owns one half (along 2nd dimension)")
        var = stream.define_variable("var", (20000, 20000), (0, 10000 * id), (20000, 10000), ctypes.sizeof(ctypes.c_double))
        engine = stream.open("cluster:my_fs:/pfs/my-working-dir/my-output", Stream.Mode.Publish)
        this_actor.info("Wait for all publishers to have opened the stream")
        this_actor.sleep_for(.5)

        this_actor.info("Start a transaction")
        engine.begin_transaction()
        this_actor.info("Put Variable 'var' into the DTL")
        engine.put(var, var.local_size)
        this_actor.info("End the transaction")
        engine.end_transaction()

        this_actor.sleep_for(1)
        this_actor.info("Close the engine")
        engine.close()
        this_actor.info("Disconnect from the DTL")
        DTL.disconnect()

    def sub_test_actor():
        dtl = DTL.connect()
        this_actor.info("Wait for 10s before becoming a Subscriber. It's not enough for the publishers to finish writing")
        this_actor.sleep_for(10)
      
        stream = dtl.add_stream("my-output")
        engine = stream.open("cluster:my_fs:/pfs/my-working-dir/my-output", Stream.Mode.Subscribe)
        var_sub = stream.inquire_variable("var")
 
        this_actor.info("Start a transaction")
        engine.begin_transaction()
        this_actor.info("Transition can start as publishers stored metadata")
        assert Engine.clock == 10
        this_actor.info("Get the entire Variable 'var' from the DTL")
        engine.get(var_sub)
        this_actor.info("End the transaction")
        engine.end_transaction()

        this_actor.info("Check local size of var_sub. Should be 3,200,000,000 bytes")
        assert var_sub.local_size == 8 * 20000 * 20000
        assert round(Engine.clock, 6) == 42.469851
        this_actor.info("Close the engine")
        engine.close()
        this_actor.info("Disconnect from the DTL")
        DTL.disconnect()
    
    for i in range(2):
        pub_hosts[i].add_actor(f"PubTestActor{i}", pub_test_actor, i)
    
    sub_host.add_actor("SubTestActor", sub_test_actor)

    e.run()

def run_test_single_pub_multiple_sub_shared_storage():
    e = setup_platform()
    pub_host = Host.by_name("node-0") 
    sub_hosts  = [Host.by_name("node-1"), Host.by_name("node-2")]

    def pub_test_actor():
        dtl = DTL.connect()
        stream = dtl.add_stream("my-output").set_engine_type(DTLEngine.Type.File).set_transport_method(Transport.Method.File)
        this_actor.info("Create a 2D-array variable with 10kx10k double")
        var = stream.define_variable("var", (10000, 10000), (0, 0), (10000, 10000), ctypes.sizeof(ctypes.c_double))
        engine = stream.open("cluster:my_fs:/pfs/my-working-dir/my-output", Stream.Mode.Publish)
        this_actor.info("Wait for all publishers to have opened the stream")
        this_actor.sleep_for(.5)

        this_actor.info("Start a transaction")
        engine.begin_transaction()
        this_actor.info("Put Variable 'var' into the DTL")
        engine.put(var, var.local_size)
        this_actor.info("End the transaction")
        engine.end_transaction()

        this_actor.sleep_for(1)
        this_actor.info("Close the engine")
        engine.close()
        this_actor.info("Disconnect from the DTL")
        DTL.disconnect()

    def sub_test_actor(id):
        dtl = DTL.connect()
        this_actor.info("Wait for 10s before becoming a Subscriber. It's not enough for the publishers to finish writing")
        this_actor.sleep_for(10)
      
        stream = dtl.add_stream("my-output")
        engine = stream.open("cluster:my_fs:/pfs/my-working-dir/my-output", Stream.Mode.Subscribe)
        var_sub = stream.inquire_variable("var")
        this_actor.info("Each publisher selects one half (along 2nd dimension) of the Variable 'var' from the DTL")
        var_sub.set_selection((0, 5000 * id), (10000, 5000))
        this_actor.info("Start a transaction")
        engine.begin_transaction()
        this_actor.info("Transition can start as publishers stored metadata")
        assert Engine.clock == 10
        this_actor.info("Get the selection")
        engine.get(var_sub)
        this_actor.info("End the transaction")
        engine.end_transaction()

        this_actor.info("Check local size of var_sub. Should be 400,000,000 bytes")
        assert var_sub.local_size == 8 * 10000 * 5000
        this_actor.info("Close the engine")
        engine.close()
        this_actor.info("Disconnect from the DTL")
        DTL.disconnect()
    
    pub_host.add_actor(f"PubTestActor", pub_test_actor)
    
    for i in range(2):
       sub_hosts[i].add_actor("SubTestActor{i}", sub_test_actor, i)

    e.run()

def run_test_set_transation_selection():
    e = setup_platform()

    def test_actor():
        dtl = DTL.connect()
        stream = dtl.add_stream("my-output").set_engine_type(DTLEngine.Type.Staging).set_transport_method(Transport.Method.Mailbox)
        this_actor.info("Create a 2D-array variable with 20kx20k double and publish it in 5 transactions")
        var = stream.define_variable("var", (20000, 20000), (0, 0), (20000, 20000), ctypes.sizeof(ctypes.c_double))
        engine = stream.open("cluster:my_fs:/node-0/scratch/my-working-dir/my-output", Stream.Mode.Publish)
        for i in range(5):
            this_actor.sleep_for(1)
            engine.begin_transaction()
            engine.put(var, var.local_size)
            engine.end_transaction()
            this_actor.sleep_for(1)

        this_actor.info("Close the engine")
        engine.close()
        DTL.disconnect()

        this_actor.info("Check that no Actor is connected to the DTL anymore")
        assert dtl.has_active_connections == False
        this_actor.info("Wait until 10s before becoming a Subscriber")
        this_actor.sleep_for(10)
        dtl = DTL.connect()
        engine = stream.open("cluster:my_fs:/node-0/scratch/my-working-dir/my-output", Stream.Mode.Subscribe)
        var_sub = stream.inquire_variable("var")
        this_actor.info("Select transaction #1 (the second one, 0 is first)")
        var_sub.set_transaction_selection(1)
        engine.begin_transaction()
        engine.get(var_sub)
        engine.end_transaction()
        this_actor.info("Select transaction #4 (the last one)")
        var_sub.set_transaction_selection(4)
        engine.begin_transaction()
        engine.get(var_sub)
        engine.end_transaction()
        this_actor.info("Check local size of var_sub. Should be 3,200,000,000 bytes")
        assert var_sub.local_size == 8 * 20000 * 20000 

        this_actor.info("Select transactions #2 and #3 (2 transactions from #2)")
        var_sub.set_transaction_selection(2, 2)
        engine.begin_transaction()
        engine.get(var_sub)
        engine.end_transaction()
        this_actor.info("Check local size of var_sub. Should be 6,400,000,000 bytes as we store 2 steps")
        assert var_sub.local_size == 2 * 8 * 20000 * 20000

        this_actor.info("Close the engine")
        engine.close()
        this_actor.info("Disconnect from the DTL")
        DTL.disconnect()
        
        Host.by_name(f"node-{i}").add_actor("TestActor", test_actor)

    e.run()

if __name__ == '__main__':
    tests = [
        run_test_single_pub_local_storage,
        run_test_single_pub_single_sub_local_storage,
        run_test_multiple_pub_single_sub_shared_storage,
        run_test_single_pub_multiple_sub_shared_storage,
        run_test_set_transation_selection
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
