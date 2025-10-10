# Copyright (c) 2025. The SWAT Team. All rights reserved.
#
# This program is free software you can redistribute it and/or modify it
# under the terms of the license (GNU LGPL) which comes with this package.

import ctypes
import sys
import multiprocessing

from simgrid import Engine, this_actor
from dtlmod import DTL, InconsistentVariableDefinitionException, MultipleVariableDefinitionException, UnknownVariableException

def setup_platform():
    e = Engine(sys.argv)
    e.set_log_control("no_loc")
    e.set_log_control("root.thresh:critical")

    root = e.netzone_root
    host = root.add_host("host", "1Gf")
    root.seal()

    DTL.create()
    return e, host

def run_test_define_variable():
    e, host = setup_platform()

    def define_variable():
        this_actor.info("Connect to the DTL")
        dtl = DTL.connect()
        this_actor.info("Create a stream")
        stream = dtl.add_stream("Stream")
        this_actor.info("Create a scalar int variable")
        stream.define_variable("scalar", ctypes.sizeof(ctypes.c_int))
        this_actor.info("Create a 3D variable")
        var3d = stream.define_variable("var3d", (64, 64, 64), (0, 0, 0), (64, 64, 64), 
                                       ctypes.sizeof(ctypes.c_double))
        this_actor.info("Check name of {var3d.name}")
        assert var3d.name == "var3d"
        this_actor.info("Check size: should be 64^3 times 8 as elements are double")
        assert var3d.global_size == (64 * 64 * 64 * 8)
        this_actor.info("Remove variable named 'var3d'. It is known, should be true")
        assert stream.remove_variable("var3d") == True
        this_actor.info("Remove variable named 'var2D'. It is unknown, should be false")
        assert stream.remove_variable("var2D") == False

        this_actor.info("Disconnect from the DTL")
        DTL.disconnect()

    host.add_actor("TestActor", define_variable)
    e.run()

def run_test_inconsistent_variable_definition():
    e, host = setup_platform()

    def inconsistent_variable_definition():
        this_actor.info("Connect to the DTL")
        dtl = DTL.connect()
        this_actor.info("Create a stream")
        stream = dtl.add_stream("Stream")
        this_actor.info("Create a 1D variable with an element count bigger than the shape, should fail.")
        try:
            stream.define_variable("var", (64), (0), (128), ctypes.sizeof(ctypes.c_double))
            assert False, "Expected InconsistentVariableDefinitionException was not raised"
        except InconsistentVariableDefinitionException:
            pass  # Test passes
        this_actor.info("Create a 3D variable with only two offsets, should fail.")
        try:
            stream.define_variable("var3d", (64, 64, 64), (0, 0), (64, 64, 64), ctypes.sizeof(ctypes.c_double))                   
            assert False, "Expected InconsistentVariableDefinitionException was not raised"
        except InconsistentVariableDefinitionException:
            pass  # Test passes
        this_actor.info("Create a 3D variable with only two element counts, should fail.")
        try:
            stream.define_variable("var3d", (64, 64, 64), (0, 0, 0), (64, 64), ctypes.sizeof(ctypes.c_double))
            assert False, "Expected InconsistentVariableDefinitionException was not raised"
        except InconsistentVariableDefinitionException:
            pass  # Test passes
        this_actor.info("Disconnect the actor from the DTL")
        DTL.disconnect()

    host.add_actor("TestActor", inconsistent_variable_definition)
    e.run()

def run_test_multi_define_variable():
    e, host = setup_platform()

    def multi_define_variable():
        dtl = DTL.connect()
        stream = dtl.add_stream("Stream")
        this_actor.info("Create a scalar int variable")
        stream.define_variable("var", ctypes.sizeof(ctypes.c_int))
        this_actor.info("Try to redefine var as a scalar double variable, which should fail")
        try:
            stream.define_variable("var", ctypes.sizeof(ctypes.c_double))
            assert False, "Expected MultipleVariableDefinitionException was not raised"
        except MultipleVariableDefinitionException:
            pass  # Test passes
        this_actor.info("Try to redefine var as a 3D variable, which should fail")
        try:
            stream.define_variable("var", (64, 64, 64), (0, 0, 0), (64, 64, 64), ctypes.sizeof(ctypes.c_double)),
            assert False, "Expected MultipleVariableDefinitionException was not raised"
        except MultipleVariableDefinitionException:
            pass  # Test passes
        this_actor.info("Define a new 3D variable")
        stream.define_variable("var3d", (64, 64, 64), (0, 0, 0), (64, 64, 64), ctypes.sizeof(ctypes.c_double))
        this_actor.info("Try to redefine var2 as a 2D variable, which should fail")
        try:
            stream.define_variable("var3d", (64, 64), (0, 0), (64, 64), ctypes.sizeof(ctypes.c_double))
            assert False, "Expected MultipleVariableDefinitionException was not raised"
        except MultipleVariableDefinitionException:
            pass  # Test passes
        this_actor.info("Try to redefine var as a 3D int variable, which should fail")
        try:
            stream.define_variable("var3d", (64, 64, 64), (0, 0, 0), (64, 64, 64), ctypes.sizeof(ctypes.c_int)),
            assert False, "Expected MultipleVariableDefinitionException was not raised"
        except MultipleVariableDefinitionException:
            pass  # Test passes
        this_actor.info("Try to redefine starts and counts which should work")
        var = stream.define_variable("var3d", (64, 64, 64), (16, 16, 16), (32, 32, 32), ctypes.sizeof(ctypes.c_double))
        this_actor.info("Check local and global sizes")
        assert var.local_size == 32 * 32 * 32 * 8
        assert var.global_size == 64 * 64 * 64 * 8

        this_actor.info("Disconnect the actor from the DTL")
        DTL.disconnect()

    host.add_actor("TestActor", multi_define_variable)
    e.run()

def run_test_distributed_variable():
    e, host = setup_platform()

    def distributed_variable_actor1():
        this_actor.info("Connect to the DTL")
        dtl = DTL.connect()
        this_actor.info("Create a stream")
        stream = dtl.add_stream("Stream")
        this_actor.info("Create a 3D variable")
        var = stream.define_variable("var", (64, 64, 64), (0, 0, 0), (48, 48, 48), ctypes.sizeof(ctypes.c_double))
        this_actor.info("Check local and global sizes")
        assert var.local_size == 48 * 48 * 48 * 8
        assert var.global_size == 64 * 64 * 64 * 8
        this_actor.info("Disconnect the actor from the DTL")
        DTL.disconnect()

    def distributed_variable_actor2():
        this_actor.info("Connect to the DTL")
        dtl = DTL.connect()
        this_actor.info("Create a stream")
        stream = dtl.add_stream("Stream")
        this_actor.info("Create a 3D variable with a different shape which should fail")
        try:
            stream.define_variable("var", (64, 64), (0, 0), (64, 64), ctypes.sizeof(ctypes.c_double)),
            assert False, "Expected MultipleVariableDefinitionException was not raised"
        except MultipleVariableDefinitionException:
            pass  # Test passes
        this_actor.info("Create a 3D variable with the same shape which should work")
        var = stream.define_variable("var", (64, 64, 64), (48, 48, 48), (16, 16, 16), ctypes.sizeof(ctypes.c_double))
        this_actor.info("Check local and global sizes")
        assert var.local_size == 16 * 16 * 16 * 8
        assert var.global_size == 64 * 64 * 64 * 8
        this_actor.info("Disconnect the actor from the DTL")
        DTL.disconnect()

    host.add_actor("TestActor1", distributed_variable_actor1)
    host.add_actor("TestActor2", distributed_variable_actor2)
    e.run()

def run_test_remove_variable():
    e, host = setup_platform()

    def remove_variable():
        this_actor.info("Connect to the DTL")
        dtl = DTL.connect()
        this_actor.info("Create a stream")
        stream = dtl.add_stream("Stream")
        this_actor.info("Create a scalar int variable")
        stream.define_variable("var", ctypes.sizeof(ctypes.c_int))
        this_actor.info("Remove variable named 'var'. It is known, should be true")
        assert stream.remove_variable("var") == True
        this_actor.info("Remove an unknown variable, which should return false")
        assert stream.remove_variable("unknow_var") == False
        this_actor.info("Disconnect the actor from the DTL")
        DTL.disconnect()

    host.add_actor("TestActor", remove_variable)
    e.run()

def run_test_inquire_variable_local():
    e, host = setup_platform()

    def inquire_variable_local():
        this_actor.info("Connect to the DTL")
        dtl = DTL.connect()
        this_actor.info("Create a stream")
        stream = dtl.add_stream("Stream")
        this_actor.info("Create a 3D variable")
        stream.define_variable("var", (64, 64, 64), (0, 0, 0), (64, 64, 64), ctypes.sizeof(ctypes.c_double))
        this_actor.info("Inquire this variable and store it in var2")
        var2 = stream.inquire_variable("var")
        this_actor.info("Check name and size of the inquired variable")
        assert var2.name == "var"
        assert var2.global_size == (64 * 64 * 64 * 8)
        this_actor.info("Inquire an unknown variable, which should raise an exception")
        try:
            stream.inquire_variable("unknow_var")
            assert False, "Expected UnknownVariableException was not raised"
        except UnknownVariableException:
            pass  # Test passes
        this_actor.info("Disconnect the actor from the DTL")
        DTL.disconnect()

    host.add_actor("TestActor", inquire_variable_local)
    e.run()

def run_test_inquire_variable_remote():
    e, host = setup_platform()

    def inquire_variable_remote_producer():
        this_actor.info("Connect to the DTL")
        dtl = DTL.connect()
        this_actor.info("Create a stream")
        stream = dtl.add_stream("Stream")
        this_actor.info("Create a 3D variable")
        stream.define_variable("var", (64, 64, 64), (0, 0, 0), (64, 64, 64), ctypes.sizeof(ctypes.c_double))
        this_actor.info("Disconnect the actor from the DTL")
        DTL.disconnect()

    def inquire_variable_remote_consumer():
        this_actor.info("Connect to the DTL")
        dtl = DTL.connect()
        this_actor.info("Create a stream")
        stream = dtl.add_stream("Stream")
        this_actor.info("Inquire the variable named 'var' created by the producer")
        var = stream.inquire_variable("var")
        this_actor.info("Check name and size of the inquired variable")
        assert var.name == "var"
        assert var.global_size == (64 * 64 * 64 * 8)
        this_actor.info("Disconnect the actor from the DTL")
        DTL.disconnect()

    host.add_actor("TestActorProducer", inquire_variable_remote_producer)
    host.add_actor("TestActorConsumer", inquire_variable_remote_consumer)
    e.run()

if __name__ == '__main__':
    tests = [
        run_test_define_variable,
        run_test_inconsistent_variable_definition,
        run_test_multi_define_variable,
        run_test_distributed_variable,
        run_test_remove_variable,
        run_test_inquire_variable_local,
        run_test_inquire_variable_remote
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
