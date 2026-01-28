.. Copyright 2025-2026

.. _DTLMod_Actors:

Simulated actors
################

The simulated actors that form your in situ processing workflow and interact with the DTL can be programmed at different
levels of complexity. This allows you to quickly develop prototypes focusing on the workflow structure and the data
flow between its components and then add complexity to each individual workflow component to increase the realism of your
simulator.

Pure S4U actors
***************

The simplest way to program a simulated actor acting as a component of your in situ processing workflow is to rely
solely on the `S4U interface of SimGrid <https://simgrid.org/doc/latest/app_s4u.html>`_. The following code examples
show you how to program a simple data publisher (distributed over multiple ranks) and subscriber (single rank).

Data publisher
^^^^^^^^^^^^^^

.. code-block:: cpp

 void distributed_publisher(int num_ranks, int rank) {
   // Connect to the DTL
   auto dtl = DTL::connect()
   // Add a ``Data'' stream  using a ``File'' engine
   auto s = dtl->add_stream("Data")
               ->set_engine_type("File")
               ->set_transport_method("File");

   // Define a 2D array of int distributed over multiple ranks
   // Each rank owns a 100 x 100 block 
   auto V = s->define_variable("V", {l_size * num_ranks, l_size},  // global shape
                                    {l_size * rank, 0},            // local offset
                                    {l_size, l_size},              // local count
                                    sizeof(int));                  // element size

   // Open the stream in ``Publish'' mode
   auto e = s->open("cluster:file_system:/working_dir/", Stream::Mode::Publish);

   for (int i = 0; i < 10 ; i++) {
     // Compute 1e3 floating point operations per element
     sg4::this_actor::execute(V->get_local_size() * 1e3);
     // Then publish ``V'' to the DTL
     e->begin_transaction();
     e->put(V);
     e->end_transaction();
   }

   // Close the engine
   e->close();
   // Disconnect from the DTL
   DTL::disconnect();
 }

Data subscriber
^^^^^^^^^^^^^^^

.. code-block:: cpp

 void subscriber() {
   auto dtl = DTL::connect()

   // Add the already defined ``Data'' stream
   auto s = dtl->add_stream("Data");
  
   // Obtain information on variable ``V''
   auto V = s->inquire_variable("V");
  
   // Open the stream in ``Subscribe'' mode
   auto e = s->open("cluster:file_system:/working_dir/", Stream::Mode::Subscribe);

   for (int i = 0; i < 10 ; i++) {
     // Get variable ``V'' from the DTL
     e->begin_transaction();
     e->get(V);
     e->end_transaction();
     // Compute 1e3 floating point operations per element
     sg4::this_actor::execute(V->get_local_size() * 1e3);
   }

   e->close();
   DTL::disconnect();
 }

Mixing MPI and S4U
******************

For instance, you can replace your ``distributed_publisher()`` actor by the following ``MPI_publisher`` whose core is
the ``for`` loop in which each actor computes a billion floating point operations, performs an all-to-all MPI
collective communication, performs more computation, and finally initiates a transaction with the DTL to publish its
share of the variable.

This example also illustrates how SMPI features can be used to reduce the memory footprint of the simulated execution.
Here, actors allocate and free a single buffer shared across all actors instead of allocating a distinct buffer each.

.. code-block:: cpp

 void MPI_publisher(int argc, char** argv) {
   MPI_Init();
   int nranks, rank;
   MPI_Comm_size(MPI_COMM_WORLD, &nranks);
   MPI_Comm_rank(MPI_COMM_WORLD, &rank);

   double size  = std::stod(argv[1]);
   double l_size = size / sqrt(nranks);

   // Connect to the DTL
   auto dtl = DTL::connect();

   // Add a ``Data'' stream  using a ``File'' engine
   auto s = dtl->add_stream("Data")
               ->set_engine_type("File")
               ->set_transport_method("File");
   
   // Define a 2D array of int
   auto v = s->define_variable("V", {size, size}, {size*rank, size*rank}, {l_size, l_size}, sizeof(int));

   // Open the stream in ``Publish'' mode
   auto e = s->open("cluster:file_system:/working_dir/", Stream::Mode::Publish);

   // Allocate a shared data buffer for MPI
   void* data = SMPI_SHARED_MALLOC(size * size);

   for (int it = 0; it < 100; it++) {
     // Compute a GFLOP
     sg4::this_actor::execute(1e9);
     // Perform an all-to-all collective communication
     MPI_Alltoall(data, size * size, MPI_CHAR, data, size * size, MPI_CHAR, MPI_COMM_WORLD);
     // More computation
     sg4::this_actor::execute(500e8);

     // publish data to the DTL
     e->begin_transaction();
     e->put(v);
     e->end_transaction();
   }

   e->close();
   DTL::disconnect();
   SMPI_SHARED_FREE(data);
   MPI_Finalize();
 }

