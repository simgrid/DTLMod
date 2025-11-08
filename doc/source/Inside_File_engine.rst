.. Copyright 2025

.. _Inside_file_engine:

Inside the File engine
######################

One of the important characteristics of the **File engine** is that publishers and subscribers can interact with the
DTL in a completely **desynchronized** way. Publishers can write down Variables into files, close the engine, and even
disconnect themselves from the DTL before subscribers start to read the Variable from these files. However, when
subscribers aim to retrieve Variables from the DTL while the publishers are exporting them, they must wait for the data
to be fully written on storage (or more precisely for the simulation of the corresponding I/O operations to be over)
before being able to read it. These two situations implies the implementation of additional checks related to which
actors (i.e., publishers and/or subscribers) are currently using the File engine and of internal synchronization
mechanisms.

On the publisher side
---------------------

The first publisher to call to :cpp:func:`begin_transaction` marks that a new :ref:`Concept_Transaction` is in progress
and increments an internal transaction counter. Then, from the second transaction using that File engine only, the
publishers check whether the previous transaction is over. To this end, the publishers are blocked on a **condition
variable**, until all the **write activities** from that transaction are completed. Once the publishers are unblocked,
they proceed with the current transaction, i.e., handling the calls to :cpp:func:`put` made in the simulator's code.

These calls to :cpp:func:`put` are delegated by the :ref:`Concept_Engine` to the selected :ref:`Concept_Transport`. For
each call to :cpp:func:`put`, a publisher simply determines, based on the part of the :ref:`Concept_Variable` it
locally owns and the **selection** made by the user, how many bytes it has to write and in which file (determined by
the name of the :ref:`Concept_Stream`) to write them. With the **default File transport method**, each publisher
creates and writes to a file called ``data.i``, where ``i`` is the unique index of the publisher.

The most important call is thus that to :cpp:func:`end_transaction` where the I/O activities are created and started.
In this function, each publisher goes over all the write operation it registered during the different calls to the
:cpp:func:`put` functions made in this transaction, and creates the corresponding simulated I/O activities by calling
the ``File::write_async()`` function of the
`FSMod file system module <https://github.com/simgrid/file-system-module>`_. These calls are made in **detached** mode,
meaning that the publisher starts an **asynchronous write**, can forget about it, and proceeds with the next Variable.
DTLMod also leverages the **signal/callback** mechanism provided by SimGrid to attach a callback triggered on the
completion of a given asynchronous write activity. This callback does two things: 1) Notify all the actors waiting for
the completion of that activity that it is now completed; and 2) Remove this activity from the list of **pending
activities** maintained by the publisher that created it.

The last action performed in :cpp:func:`end_transaction` is only done by the **last publisher** to call the function.
This actor marks the :ref:`Concept_Transaction` as over, increments an internal counter of completed transactions, and
most importantly, notifies subscribers to that Stream that this transaction is complete. Note that the fact that a
transaction is complete and the subscribers are notified does not mean that the corresponding I/O actictivites (i.e.,
writing into files) are complete and that the subscribers can start reading the files to get Variables. However, it
means that the publishers have all the publishers have determined what and where to write and that the subscribers are
allowed to use this information to create their I/O activities (i.e., reading from files) as explained in the
:ref:`File_sub_side` section.

To determine which publisher is the last to call the :cpp:func:`end_transaction` function, DTLMod relies on a
synchronization barrier for all the publishers using this Engine. This barrier is created in the very first to
:cpp:func:`end_transaction`.

The last operation performed on the publisher side of a File Engine is to **close** the engine by calling the
:cpp:func:`close` function. The publishers first have to wait for the completion of the I/O activities strated by the
last transaction performed on this engine, i.e., they are blocked on a condition variable and wait to be notified of
the respective completion of these activities. Then the last publisher to call the :cpp:func:`close` function actually
closes the Engine, as well as all the opened files on the simulated file system. Finally, if the
:cpp:func:`set_metadata_export` function has been called for the :ref:`Concept_Stream` that created the Engine, this
publisher export a summary of all the I/O operations performed during the lifetime of the Engine.

.. _File_sub_side:

On the subscriber side
----------------------

TBD
