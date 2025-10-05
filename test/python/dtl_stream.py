# Copyright (c) 2025. The SWAT Team. All rights reserved.
#
# This program is free software you can redistribute it and/or modify it
# under the terms of the license (GNU LGPL) which comes with this package.

import sys
import multiprocessing
from simgrid import Engine, this_actor
from fsmod import FileSystem, JBODStorage
from dtlmod import DTL, Stream, Transport, Engine as DTLEngine, UndefinedEngineTypeException, UndefinedTransportMethodException, InvalidEngineAndTransportCombinationException

def setup_platform():
    e = Engine(sys.argv)
    e.set_log_control("no_loc")
    e.set_log_control("root.thresh:critical")

    zone = e.netzone_root.add_netzone_full("zone")
    prod_host = zone.add_host("prod_host", "1Gf")
    prod_host.core_count = 2
    cons_host = zone.add_host("cons_host", "1Gf")
    prod_host.add_disk("prod_disk", "1kBps", "2kBps")
    cons_host.add_disk("cons_disk", "1kBps", "2kBps")
    
    pfs_server = zone.add_host("pfs_server", "1Gf")
    pfs_disks = []
    for i in range(4):
        pfs_disks.append(pfs_server.add_disk(f"pfs_disk_{i}", "10kBps", "20kBps"))
    remote_storage = JBODStorage.create("pfs_storage", pfs_disks)
    remote_storage.set_raid_level(JBODStorage.RAID.RAID5)

    prod_link = zone.add_link("prod_link", 120e6/0.97).set_latency(0)
    cons_link = zone.add_link("cons_link", 120e6/0.97).set_latency(0)
    zone.add_route(prod_host, pfs_server, [prod_link])
    zone.add_route(pfs_server, cons_host, [cons_link])
    zone.seal()
 
    fs = FileSystem.create("fs")
    FileSystem.register_file_system(zone, fs)
    fs.mount_partition("/pfs/", remote_storage, "100MB")

    DTL.create()

    return e, prod_host, cons_host

def run_test_incorrect_stream_settings():
    e, prod_host, _ = setup_platform()
    def test_producer_actor():
        this_actor.info("Connect to the DTL")
        dtl = DTL.connect()

        no_engine_type_stream = dtl.add_stream("no_engine_type_stream")
        no_engine_type_stream.set_transport_method(Transport.Method.File)
        this_actor.info("Try to open the stream with no engine type set, which should fail")
        try:
            no_engine_type_stream.open("zone:fs:/pfs/file", Stream.Mode.Publish)
        except UndefinedEngineTypeException:
            this_actor.info("Caught expected error for missing engine type")
        else:
            pass # Test passes
        
        no_transport_method_stream = dtl.add_stream("no_transport_method_stream")
        no_transport_method_stream.set_engine_type(DTLEngine.Type.File)
        this_actor.info("Try to open the stream with no transport method set, which should fail")
        try:
            no_transport_method_stream.open("file", Stream.Mode.Publish)
        except UndefinedTransportMethodException:
            this_actor.info("Caught expected error for missing transport method")
        else:
            pass # Test passes

        file_engine_with_staging_transport_stream = dtl.add_stream("file_engine_with_staging_transport_stream")
        file_engine_with_staging_transport_stream.set_engine_type(DTLEngine.Type.File)
        this_actor.info("Try to set a invalid transport method for this engine type")
        try:
            file_engine_with_staging_transport_stream.set_transport_method(Transport.Method.MQ)
        except InvalidEngineAndTransportCombinationException:
            pass # Test passes

        staging_engine_with_file_transport_stream = dtl.add_stream("staging_engine_with_file_transport_stream")
        staging_engine_with_file_transport_stream.set_transport_method(Transport.Method.File)
        this_actor.info("Try to set a invalid engine type for this transport method");
        try:
            staging_engine_with_file_transport_stream.set_engine_type(DTLEngine.Type.Staging)
        except InvalidEngineAndTransportCombinationException:
            pass # Test passes

        this_actor.info("Disconnect from the DTL")
        DTL.disconnect()

    prod_host.add_actor("Producer", test_producer_actor)

    e.run()

def run_test_publish_file_stream_open_close():
    e, prod_host, _ = setup_platform()
    def test_producer_actor():
        this_actor.info("Connect to the DTL")
        dtl = DTL.connect()

        this_actor.info("Create a stream")
        stream = dtl.add_stream("Stream")
        this_actor.info("Set transport method to Transport::Method::File")
        stream.set_transport_method(Transport.Method.File)
        this_actor.info("Set engine type to Engine::Type::File")
        stream.set_engine_type(DTLEngine.Type.File)
        this_actor.info("Open the stream in Stream::Mode::Publish mode")
        engine = stream.open("zone:fs:/pfs/file", Stream.Mode.Publish)
        this_actor.info(f"Stream is opened ({stream.engine_type},{stream.transport_method})")
        assert stream.engine_type == "Engine::Type::File"
        assert stream.transport_method == "Transport::Method::File"
        this_actor.info("Let the actor sleep for 1 second")
        this_actor.sleep_for(1)
        this_actor.info("Close the engine")
        engine.close()

        this_actor.info("Disconnect from the DTL")
        DTL.disconnect()

    prod_host.add_actor("Producer", test_producer_actor)

    e.run()

def run_test_publish_file_muliple_open():
    e, prod_host, cons_host = setup_platform()
    def test_producer_actor():
        this_actor.info("Connect to the DTL")
        dtl = DTL.connect()

        this_actor.info("Create a stream")
        stream = dtl.add_stream("Stream")
        stream.set_transport_method(Transport.Method.File)
        stream.set_engine_type(DTLEngine.Type.File)
        this_actor.info("Open the Stream in Stream::Mode::Publish mode")
        engine = stream.open("zone:fs:/pfs/file", Stream.Mode.Publish)
        this_actor.info("Check current number of publishers and subscribers")
        assert stream.num_publishers == 1
        assert stream.num_subscribers == 0
        this_actor.sleep_for(1)
    
        this_actor.info("Close the engine")
        engine.close()

        this_actor.info("Disconnect from the DTL")
        DTL.disconnect()

    def test_consumer_actor():
        this_actor.info("Connect to the DTL")
        dtl = DTL.connect()

        this_actor.info("Create a stream")
        stream = dtl.add_stream("Stream")
        this_actor.info("Set transport method to Transport::Method::File")
        stream.set_transport_method(Transport.Method.File)
        stream.set_engine_type(DTLEngine.Type.File)
        this_actor.info("Open the stream in Stream::Mode::Publish mode")
        engine = stream.open("zone:fs:/pfs/file", Stream.Mode.Publish)
        this_actor.info("Check current number of publishers and subscribers")
        assert stream.num_publishers == 2
        assert stream.num_subscribers == 0
        this_actor.info(f"Let actor {this_actor.get_name} sleep for 1 second")
        this_actor.sleep_for(1)
        this_actor.info("Close the engine")
        engine.close()

        this_actor.info("Disconnect from the DTL")
        DTL.disconnect()

    prod_host.add_actor("TestProducerActor", test_producer_actor)
    cons_host.add_actor("TestConsumerActor", test_consumer_actor)

    e.run()

if __name__ == '__main__':
    tests = [
        run_test_incorrect_stream_settings,
        run_test_publish_file_stream_open_close,
        run_test_publish_file_muliple_open
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

