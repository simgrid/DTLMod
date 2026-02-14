# Copyright (c) 2025-2026. The SWAT Team. All rights reserved.
#
# This program is free software you can redistribute it and/or modify it
# under the terms of the license (GNU LGPL) which comes with this package.

import ctypes
import math
import sys
import multiprocessing

from simgrid import Engine, Host, this_actor
from fsmod import FileSystem, OneDiskStorage
from dtlmod import (DTL, Engine as DTLEngine, Stream, Transport,
                    UnknownReductionMethodException,
                    UnknownDecimationOptionException,
                    InconsistentDecimationStrideException,
                    UnknownDecimationInterpolationException,
                    UnknownCompressionOptionException,
                    InconsistentCompressionRatioException,
                    SubscriberSideCompressionException,
                    DoubleReductionException)


def setup_platform():
    e = Engine(sys.argv)
    e.set_log_control("no_loc")
    e.set_log_control("root.thresh:critical")

    zone = e.netzone_root.add_netzone_empty("zone")
    host = zone.add_host("host", "6Gf")
    disk = host.add_disk("disk", "560MBps", "510MBps")
    zone.seal()

    my_fs = FileSystem.create("my_fs")
    FileSystem.register_file_system(zone, my_fs)
    local_storage = OneDiskStorage.create("local_storage", disk)
    my_fs.mount_partition("/host/scratch/", local_storage, "100GB")

    DTL.create()
    return e, host


def run_test_bogus_decimation_setting():
    e, host = setup_platform()

    def test_actor():
        this_actor.info("Connect to the DTL")
        dtl = DTL.connect()
        this_actor.info("Create a stream")
        stream = dtl.add_stream("my-output")
        stream.set_transport_method(Transport.Method.File)
        stream.set_engine_type(DTLEngine.Type.File)
        this_actor.info("Create a 3D variable")
        var = stream.define_variable("var3D", (640, 640, 640), (0, 0, 0), (640, 640, 640),
                                     ctypes.sizeof(ctypes.c_double))
        this_actor.info("Define an unknown Reduction Method, should fail")
        try:
            stream.define_reduction_method("reduction")
            assert False, "Expected UnknownReductionMethodException was not raised"
        except UnknownReductionMethodException:
            pass
        this_actor.info("Define a Decimation Reduction Method")
        decimator = stream.define_reduction_method("decimation")
        this_actor.info("Assign the decimation method with a bogus option, should fail")
        try:
            var.set_reduction_operation(decimator, {"bogus": "-1"})
            assert False, "Expected UnknownDecimationOptionException was not raised"
        except UnknownDecimationOptionException:
            pass
        this_actor.info("Assign the decimation method with only a 2D stride, should fail")
        try:
            var.set_reduction_operation(decimator, {"stride": "1,2"})
            assert False, "Expected InconsistentDecimationStrideException was not raised"
        except InconsistentDecimationStrideException:
            pass
        this_actor.info("Assign the decimation method with a negative stride value, should fail")
        try:
            var.set_reduction_operation(decimator, {"stride": "1,2,-1"})
            assert False, "Expected InconsistentDecimationStrideException was not raised"
        except InconsistentDecimationStrideException:
            pass
        this_actor.info("Assign the decimation method with a stride value of 0, should fail")
        try:
            var.set_reduction_operation(decimator, {"stride": "1,0,1"})
            assert False, "Expected InconsistentDecimationStrideException was not raised"
        except InconsistentDecimationStrideException:
            pass
        this_actor.info("Assign the decimation method with an unknown interpolation method, should fail")
        try:
            var.set_reduction_operation(decimator, {"stride": "1,2,4", "interpolation": "bogus"})
            assert False, "Expected UnknownDecimationInterpolationException was not raised"
        except UnknownDecimationInterpolationException:
            pass

        this_actor.info("Disconnect the actor from the DTL")
        DTL.disconnect()

    host.add_actor("TestActor", test_actor)
    e.run()


def run_test_simple_decimation_file_engine():
    e, host = setup_platform()

    def test_actor():
        this_actor.info("Connect to the DTL")
        dtl = DTL.connect()
        this_actor.info("Create a stream")
        stream = dtl.add_stream("my-output")
        stream.set_transport_method(Transport.Method.File)
        stream.set_engine_type(DTLEngine.Type.File)
        this_actor.info("Create a 3D variable")
        var = stream.define_variable("var3D", (640, 640, 640), (0, 0, 0), (640, 640, 640),
                                     ctypes.sizeof(ctypes.c_double))
        this_actor.info("Define a Decimation Reduction Method")
        decimator = stream.define_reduction_method("decimation")
        this_actor.info("Open the stream in Publish mode")
        engine = stream.open("zone:my_fs:/host/scratch/my-working-dir/my-output", Stream.Mode.Publish)
        this_actor.sleep_for(1)

        this_actor.info("Start a Transaction (no reduction)")
        engine.begin_transaction()
        engine.put(var)
        engine.end_transaction()
        this_actor.sleep_until(6)

        this_actor.info("Assign the decimation method to var3D")
        var.set_reduction_operation(decimator, {"stride": "1,2,4"})
        assert var.is_reduced
        this_actor.info("Start a Transaction (with reduction)")
        engine.begin_transaction()
        engine.put(var)
        engine.end_transaction()
        this_actor.sleep_until(8)

        this_actor.info("Triple the cost per element")
        var.set_reduction_operation(decimator, {"cost_per_element": "3"})
        engine.begin_transaction()
        engine.put(var)
        engine.end_transaction()
        this_actor.sleep_until(10)

        this_actor.info("Create a second 3D variable with different decimation")
        var2 = stream.define_variable("var3D_2", (640, 640, 640), (0, 0, 0), (640, 640, 640),
                                      ctypes.sizeof(ctypes.c_double))
        var2.set_reduction_operation(decimator, {"stride": "2,2,2", "interpolation": "quadratic"})
        engine.begin_transaction()
        engine.put(var2)
        engine.end_transaction()

        this_actor.info("Close the engine")
        engine.close()
        this_actor.info("Disconnect the actor from the DTL")
        DTL.disconnect()

    host.add_actor("TestActor", test_actor)
    e.run()


def run_test_single_pub_single_sub_decimation_on_read():
    e, host = setup_platform()

    def test_actor():
        dtl = DTL.connect()
        stream = dtl.add_stream("my-output")
        stream.set_transport_method(Transport.Method.File)
        stream.set_engine_type(DTLEngine.Type.File)
        this_actor.info("Create a 2D variable with 20kx20k doubles")
        var = stream.define_variable("var", (20000, 20000), (0, 0), (20000, 20000), ctypes.sizeof(ctypes.c_double))
        engine = stream.open("zone:my_fs:/host/scratch/my-working-dir/my-output", Stream.Mode.Publish)
        this_actor.sleep_for(1)

        this_actor.info("Put the variable")
        engine.begin_transaction()
        engine.put(var)
        engine.end_transaction()

        this_actor.info("Close the engine")
        engine.close()
        DTL.disconnect()

        assert dtl.has_active_connections == False

        this_actor.info("Wait until 10s before becoming a Subscriber")
        this_actor.sleep_until(10)
        dtl = DTL.connect()

        this_actor.info("Define a Decimation Reduction Method on Subscriber side")
        decimator = stream.define_reduction_method("decimation")
        engine = stream.open("zone:my_fs:/host/scratch/my-working-dir/my-output", Stream.Mode.Subscribe)
        var_sub = stream.inquire_variable("var")
        assert var_sub.name == "var"
        assert var_sub.global_size == 8 * 20000 * 20000

        this_actor.info("Get the entire variable (no reduction)")
        engine.begin_transaction()
        engine.get(var_sub)
        engine.end_transaction()

        this_actor.info("Assign the decimation method to var_sub")
        var_sub.set_reduction_operation(decimator, {"stride": "2,2"})

        this_actor.info("Get a decimated version of the variable")
        engine.begin_transaction()
        engine.get(var_sub)
        engine.end_transaction()

        this_actor.info("Check local size of var_sub. Should be 800,000,000 bytes")
        assert var_sub.local_size == 8 * 10000 * 10000

        this_actor.info("Close the engine")
        engine.close()
        this_actor.info("Disconnect the actor")
        DTL.disconnect()

    host.add_actor("TestActor", test_actor)
    e.run()


def run_test_bogus_compression_setting():
    e, host = setup_platform()

    def test_actor():
        this_actor.info("Connect to the DTL")
        dtl = DTL.connect()
        this_actor.info("Create a stream")
        stream = dtl.add_stream("my-output")
        stream.set_transport_method(Transport.Method.File)
        stream.set_engine_type(DTLEngine.Type.File)
        this_actor.info("Create a 3D variable")
        var = stream.define_variable("var3D", (640, 640, 640), (0, 0, 0), (640, 640, 640),
                                     ctypes.sizeof(ctypes.c_double))
        this_actor.info("Define a Compression Reduction Method")
        compressor = stream.define_reduction_method("compression")

        this_actor.info("Assign the compression method with a bogus option, should fail")
        try:
            var.set_reduction_operation(compressor, {"bogus": "1"})
            assert False, "Expected UnknownCompressionOptionException was not raised"
        except UnknownCompressionOptionException:
            pass
        this_actor.info("Assign with 'fixed' profile but no ratio, should fail")
        try:
            var.set_reduction_operation(compressor, {"compressor": "fixed"})
            assert False, "Expected InconsistentCompressionRatioException was not raised"
        except InconsistentCompressionRatioException:
            pass
        this_actor.info("Assign with ratio < 1, should fail")
        try:
            var.set_reduction_operation(compressor, {"compression_ratio": "0.5"})
            assert False, "Expected InconsistentCompressionRatioException was not raised"
        except InconsistentCompressionRatioException:
            pass
        this_actor.info("Assign with unknown compressor profile, should fail")
        try:
            var.set_reduction_operation(compressor, {"compressor": "bogus"})
            assert False, "Expected UnknownCompressionOptionException was not raised"
        except UnknownCompressionOptionException:
            pass

        this_actor.info("Disconnect the actor from the DTL")
        DTL.disconnect()

    host.add_actor("TestActor", test_actor)
    e.run()


def run_test_simple_compression_file_engine():
    e, host = setup_platform()

    def test_actor():
        this_actor.info("Connect to the DTL")
        dtl = DTL.connect()
        this_actor.info("Create a stream")
        stream = dtl.add_stream("my-output")
        stream.set_transport_method(Transport.Method.File)
        stream.set_engine_type(DTLEngine.Type.File)
        this_actor.info("Create a 2D variable with 1000x1000 doubles")
        var = stream.define_variable("var2D", (1000, 1000), (0, 0), (1000, 1000), ctypes.sizeof(ctypes.c_double))
        this_actor.info("Define a Compression Reduction Method")
        compressor = stream.define_reduction_method("compression")

        this_actor.info("Open the stream in Publish mode")
        engine = stream.open("zone:my_fs:/host/scratch/my-working-dir/my-output", Stream.Mode.Publish)
        this_actor.sleep_for(1)

        this_actor.info("Assign compression with fixed ratio of 10")
        var.set_reduction_operation(compressor, {"compression_ratio": "10",
                                                 "compression_cost_per_element": "5",
                                                 "decompression_cost_per_element": "2"})
        assert var.is_reduced
        assert var.is_reduced_by_publisher

        this_actor.info("Verify reduced sizes")
        original_global_size = ctypes.sizeof(ctypes.c_double) * 1000 * 1000
        expected_reduced = math.ceil(original_global_size / 10.0)
        assert compressor.get_reduced_variable_global_size(var) == expected_reduced
        assert compressor.get_reduced_variable_local_size(var) == expected_reduced

        this_actor.info("Verify that shape is unchanged")
        reduced_shape = compressor.get_reduced_variable_shape(var)
        assert len(reduced_shape) == 2
        assert reduced_shape[0] == 1000
        assert reduced_shape[1] == 1000

        this_actor.info("Verify compression flop cost")
        expected_flops = 5.0 * 1000 * 1000
        assert compressor.get_flop_amount_to_reduce_variable(var) == expected_flops

        this_actor.info("Verify decompression flop cost")
        expected_decomp_flops = 2.0 * 1000 * 1000
        assert compressor.get_flop_amount_to_decompress_variable(var) == expected_decomp_flops

        engine.begin_transaction()
        engine.put(var)
        engine.end_transaction()
        this_actor.sleep_for(1)
        engine.close()

        this_actor.info("Disconnect the actor from the DTL")
        DTL.disconnect()

    host.add_actor("Publisher", test_actor)
    e.run()


def run_test_compression_with_derived_ratio():
    e, host = setup_platform()

    def test_actor():
        this_actor.info("Connect to the DTL")
        dtl = DTL.connect()
        stream = dtl.add_stream("my-output")
        stream.set_transport_method(Transport.Method.File)
        stream.set_engine_type(DTLEngine.Type.File)
        var = stream.define_variable("var2D", (1000, 1000), (0, 0), (1000, 1000), ctypes.sizeof(ctypes.c_double))
        orig_size = ctypes.sizeof(ctypes.c_double) * 1000 * 1000

        this_actor.info("Test SZ profile: accuracy=1e-3, data_smoothness=0.5")
        sz_compressor = stream.define_reduction_method("compression")
        var.set_reduction_operation(sz_compressor, {"compressor": "sz", "accuracy": "1e-3", "data_smoothness": "0.5"})
        assert var.is_reduced
        sz_reduced = sz_compressor.get_reduced_variable_global_size(var)
        assert sz_reduced > 0
        assert sz_reduced < orig_size
        this_actor.info(f"SZ reduced size: {sz_reduced} (original: {orig_size}, ratio: {orig_size / sz_reduced:.2f})")

        this_actor.info("Test ZFP profile: accuracy=1e-6")
        stream2 = dtl.add_stream("my-output-2")
        stream2.set_transport_method(Transport.Method.File)
        stream2.set_engine_type(DTLEngine.Type.File)
        var2 = stream2.define_variable("var2D", (1000, 1000), (0, 0), (1000, 1000), ctypes.sizeof(ctypes.c_double))
        zfp_compressor = stream2.define_reduction_method("compression")
        var2.set_reduction_operation(zfp_compressor, {"compressor": "zfp", "accuracy": "1e-6"})
        zfp_reduced = zfp_compressor.get_reduced_variable_global_size(var2)
        assert zfp_reduced > 0
        assert zfp_reduced < orig_size
        this_actor.info(f"ZFP reduced size: {zfp_reduced} (original: {orig_size}, ratio: {orig_size / zfp_reduced:.2f})")

        this_actor.info("Verify SZ gives higher compression than ZFP at these settings")
        assert sz_reduced < zfp_reduced

        this_actor.info("Disconnect the actor from the DTL")
        DTL.disconnect()

    host.add_actor("TestActor", test_actor)
    e.run()


def run_test_double_reduction_forbidden():
    e, host = setup_platform()

    def test_actor():
        this_actor.info("Connect to the DTL")
        dtl = DTL.connect()
        stream = dtl.add_stream("my-output")
        stream.set_transport_method(Transport.Method.File)
        stream.set_engine_type(DTLEngine.Type.File)
        var = stream.define_variable("var", (20000, 20000), (0, 0), (20000, 20000), ctypes.sizeof(ctypes.c_double))
        compressor = stream.define_reduction_method("compression")
        engine = stream.open("zone:my_fs:/host/scratch/my-working-dir/my-output", Stream.Mode.Publish)
        this_actor.sleep_for(1)

        this_actor.info("Apply publisher-side compression")
        var.set_reduction_operation(compressor, {"compression_ratio": "5"})
        assert var.is_reduced_by_publisher

        this_actor.info("Re-parameterize the same reduction method (allowed)")
        var.set_reduction_operation(compressor, {"compression_ratio": "10"})
        assert var.is_reduced_by_publisher

        engine.begin_transaction()
        engine.put(var)
        engine.end_transaction()
        this_actor.sleep_for(1)
        engine.close()
        DTL.disconnect()

        this_actor.info("Wait and reconnect as subscriber")
        this_actor.sleep_until(10)
        dtl = DTL.connect()
        engine = stream.open("zone:my_fs:/host/scratch/my-working-dir/my-output", Stream.Mode.Subscribe)
        var_sub = stream.inquire_variable("var")

        this_actor.info("Verify that var_sub carries publisher reduction state")
        assert var_sub.is_reduced
        assert var_sub.is_reduced_by_publisher

        this_actor.info("Attempt subscriber-side compression, should fail")
        sub_compressor = stream.define_reduction_method("compression")
        try:
            var_sub.set_reduction_operation(sub_compressor, {"compression_ratio": "2"})
            assert False, "Expected SubscriberSideCompressionException was not raised"
        except SubscriberSideCompressionException:
            pass

        this_actor.info("Attempt subscriber-side decimation on publisher-reduced variable, should fail")
        decimator = stream.define_reduction_method("decimation")
        try:
            var_sub.set_reduction_operation(decimator, {"stride": "2,2"})
            assert False, "Expected DoubleReductionException was not raised"
        except DoubleReductionException:
            pass

        engine.close()
        DTL.disconnect()

    host.add_actor("TestActor", test_actor)
    e.run()


def setup_staging_platform():
    """Set up a two-host platform with network links for staging engine tests."""
    e = Engine(sys.argv)
    e.set_log_control("no_loc")
    e.set_log_control("root.thresh:critical")

    zone = e.netzone_root.add_netzone_star("zone")
    pub_host = zone.add_host("pub_host", "6Gf")
    sub_host = zone.add_host("sub_host", "6Gf")
    backbone = zone.add_link("backbone", "10Gbps").set_latency("10us")
    link_pub = zone.add_link("link_pub", "10Gbps").set_latency("10us")
    link_sub = zone.add_link("link_sub", "10Gbps").set_latency("10us")
    zone.add_route(pub_host, None, [link_pub, backbone])
    zone.add_route(sub_host, None, [link_sub, backbone])
    zone.seal()

    DTL.create()
    return e


def run_test_decimation_staging_engine():
    e = setup_staging_platform()
    pub_host = Host.by_name("pub_host")
    sub_host = Host.by_name("sub_host")

    def publisher():
        dtl = DTL.connect()
        stream = dtl.add_stream("my-output")
        stream.set_engine_type(DTLEngine.Type.Staging)
        stream.set_transport_method(Transport.Method.MQ)
        this_actor.info("Create a 2D variable with 10kx10k doubles")
        var = stream.define_variable("var", (10000, 10000), (0, 0), (10000, 10000), ctypes.sizeof(ctypes.c_double))
        decimator = stream.define_reduction_method("decimation")
        engine = stream.open("my-output", Stream.Mode.Publish)
        this_actor.sleep_for(0.5)

        this_actor.info("Assign decimation with stride 2,2")
        var.set_reduction_operation(decimator, {"stride": "2,2"})
        assert var.is_reduced
        assert var.is_reduced_by_publisher

        this_actor.info("Verify reduced shape: 5000x5000")
        shape = decimator.get_reduced_variable_shape(var)
        assert shape[0] == 5000
        assert shape[1] == 5000

        engine.begin_transaction()
        engine.put(var)
        engine.end_transaction()
        this_actor.sleep_for(1)
        engine.close()
        DTL.disconnect()

    def subscriber():
        dtl = DTL.connect()
        stream = dtl.add_stream("my-output")
        engine = stream.open("my-output", Stream.Mode.Subscribe)
        var = stream.inquire_variable("var")

        this_actor.info("Get the decimated variable")
        engine.begin_transaction()
        engine.get(var)
        engine.end_transaction()

        engine.close()
        DTL.disconnect()

    pub_host.add_actor("Publisher", publisher)
    sub_host.add_actor("Subscriber", subscriber)
    e.run()


def run_test_compression_staging_engine():
    e = setup_staging_platform()
    pub_host = Host.by_name("pub_host")
    sub_host = Host.by_name("sub_host")

    def publisher():
        dtl = DTL.connect()
        stream = dtl.add_stream("my-output")
        stream.set_engine_type(DTLEngine.Type.Staging)
        stream.set_transport_method(Transport.Method.MQ)
        this_actor.info("Create a 2D variable with 10kx10k doubles")
        var = stream.define_variable("var", (10000, 10000), (0, 0), (10000, 10000), ctypes.sizeof(ctypes.c_double))
        compressor = stream.define_reduction_method("compression")
        engine = stream.open("my-output", Stream.Mode.Publish)
        this_actor.sleep_for(0.5)

        this_actor.info("Assign compression with ratio 5 and explicit costs")
        var.set_reduction_operation(compressor, {"compression_ratio": "5",
                                                 "compression_cost_per_element": "3",
                                                 "decompression_cost_per_element": "1"})
        assert var.is_reduced
        assert var.is_reduced_by_publisher

        this_actor.info("Verify compressed sizes")
        expected_reduced = math.ceil(ctypes.sizeof(ctypes.c_double) * 10000.0 * 10000.0 / 5.0)
        assert compressor.get_reduced_variable_global_size(var) == expected_reduced
        assert compressor.get_reduced_variable_local_size(var) == expected_reduced

        this_actor.info("Verify shape is unchanged")
        shape = compressor.get_reduced_variable_shape(var)
        assert shape[0] == 10000
        assert shape[1] == 10000

        engine.begin_transaction()
        engine.put(var)
        engine.end_transaction()
        this_actor.sleep_for(1)
        engine.close()
        DTL.disconnect()

    def subscriber():
        dtl = DTL.connect()
        stream = dtl.add_stream("my-output")
        engine = stream.open("my-output", Stream.Mode.Subscribe)
        var = stream.inquire_variable("var")

        this_actor.info("Get the compressed variable (decompression cost should be applied)")
        engine.begin_transaction()
        engine.get(var)
        engine.end_transaction()

        engine.close()
        DTL.disconnect()

    pub_host.add_actor("Publisher", publisher)
    sub_host.add_actor("Subscriber", subscriber)
    e.run()


if __name__ == '__main__':
    tests = [
        run_test_bogus_decimation_setting,
        run_test_simple_decimation_file_engine,
        run_test_single_pub_single_sub_decimation_on_read,
        run_test_bogus_compression_setting,
        run_test_simple_compression_file_engine,
        run_test_compression_with_derived_ratio,
        run_test_double_reduction_forbidden,
        run_test_decimation_staging_engine,
        run_test_compression_staging_engine,
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
