.. Copyright 2025-2026

.. _Workflow:

Designing your simulated in situ processing workflow
====================================================

Resorting to simulation for the performance evaluation of in situ processing workflows allows you to: (i) capture
complex, dynamic, and transient performance behaviors (e.g., network and I/O contention or dependencies between
workflow components) that may cause unexpected waiting times; and (ii) to abstract the computational complexity of
the various workflow components.

The choice to build DTLMod on top of `SimGrid <https://simgrid.org>`_ was motivated by the fact that SimGrid does not
only provides a **fast simulation kernel** and **validated network models** but also allows developers of simulators to
combine different programming models (e.g., data-flow applications represented as directed acyclic graphs, 
communicating sequential processes, or MPI codes) within the same
`simulator <https://github.com/simgrid/simgrid_frankenstein>`_.

The most abstract way to simulate a workflow component is to implement a very simple actor that just sleeps for a
certain amount of time. It is also possible to define simulated actors that mix simulated activities (e.g., 
computations) to MPI calls to mimic the execution flow of an MPI application without having to emulate the exact
computations it performs. The simulation of these mock MPI applications reproduces complex communication patterns
while avoiding the full complexity of the real-world application. Finally, SimGrid's `SMPI programming interface
<https://simgrid.org/doc/latest/app_smpi.html>`_ allows you to simulate of full-fledged, unmodified parallel MPI
applications as individual workflow components.

Regardless of the level of abstraction and complexity of the simulated workflow components, DTLMod allows you to
connect these components through the **data they exchange** to form an in situ processing workflow. Note that when
simulating parallel MPI applications that are already interfaced with a data management framework to handle their I/Os,
enabling their simulated execution with DTLMod simply amounts to replace the sections of code interacting with these
data management frameworks, which are usually well identified and isolated, by calls to DTLMod.

In the remaining of this section of the documentation, we will introduce the main concepts of DTLMod, present its API,
and illustrate how to design a simulated in situ processing workflow through a simple example.