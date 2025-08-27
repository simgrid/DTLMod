/* Copyright (c) 2025. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

 #include <pybind11/pybind11.h> // Must come before our own stuff

#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include <dtlmod/DTL.hpp>
#include <dtlmod/DTLException.hpp>
#include <dtlmod/Engine.hpp>
#include <dtlmod/FileEngine.hpp>
#include <dtlmod/FileTransport.hpp>
#include <dtlmod/Metadata.hpp>
#include <dtlmod/StagingEngine.hpp>
#include <dtlmod/StagingMboxTransport.hpp>
#include <dtlmod/StagingMqTransport.hpp>
#include <dtlmod/StagingTransport.hpp>
#include <dtlmod/Stream.hpp>
#include <dtlmod/Transport.hpp>
#include <dtlmod/Variable.hpp>

#include <xbt/log.h>

namespace py = pybind11;
using dtlmod::DTL;
using dtlmod::Engine;
using dtlmod::Stream;
using dtlmod::Variable;

XBT_LOG_NEW_DEFAULT_CATEGORY(python, "python");


namespace {

std::string get_dtlmod_version()
{
  int major;
  int minor;
  //sg_version_get(&major, &minor, &patch);
  return simgrid::xbt::string_printf("%i.%i", 0, 1);//major, minor, patch);
}
} // namespace

PYBIND11_MODULE(dtlmod, m)
{
  m.doc() = "DTLMod userspace API";

  m.attr("dtlmod_version") = get_dtlmod_version();

  py::register_exception<dtlmod::DTLInitBadSenderException>(m, "DTLInitBadSenderException");
  //TODO add other exceptions (when needed)

  /* Class DTL */
  py::class_<DTL>(m, "DTL", "Data Transport Layer")
    .def_static("create", static_cast<void(*)()>(&DTL::create), "Create the DTL")
    .def_static("create", static_cast<void(*)(const std::string&)>(&DTL::create), py::arg("filename"), 
        "Create the DTL")
    .def_static("connect", static_cast<std::shared_ptr<DTL>(*)()>(&DTL::connect), 
        "Connect an Actor to the DTL")
    .def_static("disconnect", static_cast<void(*)()>(&DTL::disconnect),
        "Disconnect an Actor from the DTL")
    .def_property_readonly("has_active_connections", &DTL::has_active_connections, 
        "Check whether some simulated actors are currently connected to the DTL")
    .def("add_stream", &DTL::add_stream, py::arg("name"), "Add a data stream to the DTL")
    .def_property_readonly("get_all_streams", &DTL::get_all_streams, 
        "Retrieve all streams declared in the DTL")
    .def_property_readonly("get_stream_by_name_or_null", &DTL::get_stream_by_name_or_null, 
        "Retrieve a data stream from the DTL by its name");
}