.. Copyright 2025-2026

.. _DTLMod_Main:

Composing simulated actors and running the simulation
#####################################################

The code below shows how to create and run a simulation-analysis in situ processing workflow using DTLMod to exchange
data between the ``MPI_publisher`` and the ``subscriber`` that runs on a single core. 

.. code-block:: cpp

 int main(int argc, char** argv) {
   sg4::Engine e(&argc, argv);
   e.load_platform("./platform.so");

   auto sub_host = e.get_host_by_name("node-0");
   auto pub_hosts = e.get_hosts_from_MPI_hostfile("./hostfile");

   // Create the data transport layer
   DTL::create("./DTL_config_file.json");

   // Start a simulated MPI code instance run by multiple actors
   SMPI_app_instance_start("MPI_publisher", MPI_publisher, pub_hosts, argc, argv);

   // Create a single in situ analysis actor
   e.add_actor("subscriber", sub_host, subscriber);

   // Run the simulation
   e.run();

   return 0;
 }

This code first creates the SimGrid engine in charge of the execution of the simulation and loads a shared library that
describes the simulated platform. Then, it assigns roles to hosts (i.e., simulated compute nodes) in that platform. The
subscriber component is executed on ``node-0`` while the locations where the MPI ranks will run are defined in a regular MPI hostfile. 

The DTL is created by calling :cpp:func:`DTL::create()` (line 21). An instance of the MPI publisher and the subscriber
actor are then created before running the simulation.

In situ processing workflows involving more than two components can be simulated using the same principles:

1. Each component is either an MPI code or a simulated actor.
2. Each component creates one or more streams to publish and/or subscribe to data.
3. The streams create the flow dependencies between components and form the workflow.

