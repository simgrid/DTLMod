# Copyright (c) 2025-2026. The SWAT Team. All rights reserved.
#
# This program is free software you can redistribute it and/or modify it
# under the terms of the license (GNU LGPL) which comes with this package.

import sys
import multiprocessing
from simgrid import Engine, this_actor
from fsmod import FileSystem, OneDiskStorage
from dtlmod import DTL, Stream

def setup_platform():
    e = Engine(sys.argv)
    e.set_log_control("no_loc")
    e.set_log_control("root.thresh:critical")

    root = e.netzone_root.add_netzone_full("root")
    host = root.add_host("host", "1Gf")
    disk = host.add_disk("disk", "1kBps", "2kBps")
    root.seal()

    local_storage = OneDiskStorage.create("local_storage", disk)
    fs = FileSystem.create("fs")
    FileSystem.register_file_system(root, fs)
    fs.mount_partition("/scratch/", local_storage, "100MB")

    DTL.create("../../config_files/test/DTL-config.json")
    return e, host

def run_test_config_file():
    e, host = setup_platform()
    def test_config_file():
        this_actor.info("Connect to the DTL")
        dtl = DTL.connect()
        this_actor.info("Create a stream")
        stream = dtl.add_stream("Stream1")
        this_actor.info("Open the stream")
        engine = stream.open("root:fs:/scratch/file", Stream.Mode.Publish)
        this_actor.info(f"Stream 1 is opened ({stream.engine_type},{stream.transport_method})")
        assert stream.engine_type == "Engine::Type::File"
        assert stream.transport_method == "Transport::Method::File"
        assert stream.access_mode == "Mode::Publish"
        this_actor.info("Check if this stream is set to export metadata (it is)")
        assert True == stream.metadata_export
        this_actor.info("Change the metadata export setting and check again")
        stream.unset_metadata_export()
        assert False == stream.metadata_export
        this_actor.info("Let the actor sleep for 1 second")
        this_actor.sleep_for(1)
        this_actor.info("Close the engine")
        engine.close()

        this_actor.info("Get a stream by name from the DTL")
        stream = dtl.stream_by_name_or_null("Stream2")
        this_actor.info("Try to get a stream by name from the DTL that doesn't exist. Should return None")
        assert None == dtl.stream_by_name_or_null("Unknown Stream")
        this_actor.info("Open the stream")
        engine = stream.open("staging", Stream.Mode.Publish)
        this_actor.info(f"Stream 1 is opened ({stream.engine_type},{stream.transport_method})")
        assert stream.engine_type == "Engine::Type::Staging"
        assert stream.transport_method == "Transport::Method::MQ"
        this_actor.info("Let the actor sleep for 1 second")
        this_actor.sleep_for(1)
        this_actor.info("Close the engine")
        engine.close()

        this_actor.info("Disconnect from the DTL")
        DTL.disconnect()

    host.add_actor("TestActor", test_config_file)
    e.run()

if __name__ == '__main__':
    tests = [
        run_test_config_file
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
