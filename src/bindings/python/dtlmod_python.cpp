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
#include <dtlmod/version.hpp>

#include <xbt/log.h>

namespace py = pybind11;
using dtlmod::DTL;
using dtlmod::Engine;
using dtlmod::Stream;
using dtlmod::Transport;
using dtlmod::Variable;

XBT_LOG_NEW_DEFAULT_CATEGORY(python, "python");

namespace {

std::string get_dtlmod_version()
{
  int major;
  int minor;
  int patch;
  dtlmod_version_get(&major, &minor, &patch);
  return simgrid::xbt::string_printf("%i.%i.%i", major, minor, patch);
}
} // namespace

PYBIND11_MODULE(dtlmod, m)
{
  m.doc() = "DTLMod userspace API";

  m.attr("dtlmod_version") = get_dtlmod_version();

  py::register_exception<dtlmod::DTLInitBadSenderException>(m, "DTLInitBadSenderException");

  py::register_exception<dtlmod::UnkownEngineTypeException>(m, "UnkownEngineTypeException");
  py::register_exception<dtlmod::UndefinedEngineTypeException>(m, "UndefinedEngineTypeException");
  py::register_exception<dtlmod::MultipleEngineTypeException>(m, "MultipleEngineTypeException");

  py::register_exception<dtlmod::UnknownTransportMethodException>(m, "UnknownTransportMethodException");
  py::register_exception<dtlmod::UndefinedTransportMethodException>(m, "UndefinedTransportMethodException");
  py::register_exception<dtlmod::MultipleTransportMethodException>(m, "MultipleTransportMethodException");

  py::register_exception<dtlmod::InvalidEngineAndTransportCombinationException>(
      m, "InvalidEngineAndTransportCombinationException");
  py::register_exception<dtlmod::OpenStreamFailureException>(m, "OpenStreamFailureException");

  py::register_exception<dtlmod::UnknownOpenModeException>(m, "UnknownOpenModeException");

  py::register_exception<dtlmod::UnknownVariableException>(m, "UnknownVariableException");
  py::register_exception<dtlmod::MultipleVariableDefinitionException>(m, "MultipleVariableDefinitionException");
  py::register_exception<dtlmod::InconsistentVariableDefinitionException>(m, "InconsistentVariableDefinitionException");

  py::register_exception<dtlmod::IncorrectPathDefinitionException>(m, "IncorrectPathDefinitionException");

  py::register_exception<dtlmod::GetWhenNoTransactionException>(m, "GetWhenNoTransactionException");

  /* Class DTL */
  py::class_<DTL, std::shared_ptr<DTL>>(m, "DTL", "Data Transport Layer")
      .def_static("create", py::overload_cast<>(&DTL::create), py::call_guard<py::gil_scoped_release>(),
                  "Create the DTL (no return)")
      .def_static("create", py::overload_cast<const std::string&>(&DTL::create),
                  py::call_guard<py::gil_scoped_release>(), py::arg("filename"), "Create the DTL (no return)")
      .def_static("connect", py::overload_cast<>(&DTL::connect), py::call_guard<py::gil_scoped_release>(),
                  "Connect an Actor to the DTL")
      .def_static("disconnect", py::overload_cast<>(&DTL::disconnect), py::call_guard<py::gil_scoped_release>(),
                  "Disconnect an Actor from the DTL")
      .def_property_readonly("has_active_connections", &DTL::has_active_connections,
                             "Check whether some simulated actors are currently connected to the DTL (read-only)")
      .def("add_stream", &DTL::add_stream, py::call_guard<py::gil_scoped_release>(), py::arg("name"),
           "Add a data stream to the DTL")
      .def_property_readonly("get_all_streams", &DTL::get_all_streams,
                             "Retrieve all streams declared in the DTL (read-only)")
      .def_property_readonly("get_stream_by_name_or_null", &DTL::get_stream_by_name_or_null,
                             "Retrieve a data stream from the DTL by its name  (read-only)");

  /* Class Stream */
  py::class_<Stream, std::shared_ptr<Stream>> stream(
      m, "Stream", "A Stream defines the connection between the applications that produce or consume data and the DTL");
  stream.def_property_readonly("name", &Stream::get_name, "The name of the Stream (read-only)")
      .def_property_readonly("engine_type", &Stream::get_engine_type_str,
                             "Print out the engine type of this Stream (read-only)")
      .def_property_readonly("transport_method", &Stream::get_transport_method_str,
                             "Print out the transport method of this Stream (read-only)")
      .def("set_engine_type", &Stream::set_engine_type, py::arg("type"),
           "Set the engine type associated to this Stream")
      .def("set_transport_method", &Stream::set_transport_method, py::arg("method"),
           "Set the transport method associated to this Stream")
      // Engine factory
      .def("open", &Stream::open, py::arg("name"), py::call_guard<py::gil_scoped_release>(), py::arg("mode"),
           "Open a Stream and create an Engine")
      .def_property_readonly("num_publishers", &Stream::get_num_publishers,
                             "The number of actors connected to this Stream in Mode::Publish (read-only)")
      .def_property_readonly("num_subscribers", &Stream::get_num_subscribers,
                             "The number of actors connected to this Stream in Mode::Subscribe (read-only)")
      // Variable factory
      .def(
          "define_variable",
          [](Stream& self, const std::string& name, size_t element_size) {
            return self.define_variable(name, element_size);
          },
          py::call_guard<py::gil_scoped_release>(), py::arg("name"), py::arg("element_size"),
          "Define a scalar variable for this Stream")
      .def(
          "define_variable",
          [](Stream& self, const std::string& name, const std::vector<size_t>& shape, const std::vector<size_t>& start,
             const std::vector<size_t>& count,
             size_t element_size) { return self.define_variable(name, shape, start, count, element_size); },
          py::call_guard<py::gil_scoped_release>(), py::arg("name"), py::arg("shape"), py::arg("start"),
          py::arg("count"), py::arg("element_size"), "Define a variable for this Stream")
      .def("inquire_variable", &Stream::inquire_variable, py::arg("name"), "Retrieve a Variable information by name")
      .def("remove_variable", &Stream::remove_variable, py::arg("name"), "Remove a Variable from this Stream");

  py::enum_<Stream::Mode>(stream, "Mode", "The access mode for a Stream")
      .value("Publish", Stream::Mode::Publish)
      .value("Subscribe", Stream::Mode::Subscribe);

  /* Class Variable */
  py::class_<Variable, std::shared_ptr<Variable>>(
      m, "Variable", "A Variable defines a data object that can be injected into or retrieved from a Stream")
      .def_property_readonly("name", &Variable::get_name, "The name of the Variable (read-only)")
      .def_property_readonly("shape", &Variable::get_shape, "The shape of the Variable (read-only)")
      .def_property_readonly("element_size", &Variable::get_element_size,
                             "The element size of the Variable (read-only)")
      .def_property_readonly("local_size", &Variable::get_local_size,
                             "The local size of the Variable for the current actor (read-only)")
      .def_property_readonly("global_size", &Variable::get_global_size, "The global size of the Variable (read-only)")
      .def(
          "set_transaction_selection",
          [](Variable& self, unsigned int transaction_id) { self.set_transaction_selection(transaction_id); },
          py::arg("transaction_id"), "Set the selection of transactions to consider for this Variable")
      .def(
          "set_transaction_selection",
          [](Variable& self, unsigned int begin, unsigned int count) { self.set_transaction_selection(begin, count); },
          py::arg("begin"), py::arg("count"), "Set the selection of transactions to consider for this Variable")
      .def("set_selection", &Variable::set_selection, py::arg("start"), py::arg("count"),
           "Set the selection of elements to consider for this Variable");

  /* Class Engine */
  py::class_<Engine, std::shared_ptr<Engine>> engine(
      m, "Engine", "An Engine defines how data is transferred between the applications and the DTL");
  engine.def_property_readonly("name", &Engine::get_name, "The name of the Engine (read-only)")
      .def("begin_transaction", &Engine::begin_transaction, py::call_guard<py::gil_scoped_release>(),
           "Begin a transaction on this Engine")
      .def("put", &Engine::put, py::arg("var"), py::call_guard<py::gil_scoped_release>(),
           py::arg("simulated_size_in_bytes"), "Put a Variable in the DTL using this Engine")
      .def("get", &Engine::get, py::arg("var"), py::call_guard<py::gil_scoped_release>(),
           "Get a Variable from the DTL using this Engine")
      .def("end_transaction", &Engine::end_transaction, py::call_guard<py::gil_scoped_release>(),
           "End a transaction on this Engine")
      .def_property_readonly("current_transaction", &Engine::get_current_transaction,
                             "The id of the current transaction on this Engine (read-only)")
      .def("close", &Engine::close, py::call_guard<py::gil_scoped_release>(), "Close this Engine");

  py::enum_<Engine::Type>(engine, "Type", "The type of Engine")
      .value("Undefined", Engine::Type::Undefined)
      .value("Staging", Engine::Type::Staging)
      .value("File", Engine::Type::File);

  /* Class Transport */
  py::class_<Transport> transport(m, "Transport", "The transport method used by an Engine to transfer data");
  py::enum_<Transport::Method>(transport, "Method", "The transport method used by the Engine")
      .value("Undefined", Transport::Method::Undefined)
      .value("MQ", Transport::Method::MQ)
      .value("Mailbox", Transport::Method::Mailbox)
      .value("File", Transport::Method::File);
}