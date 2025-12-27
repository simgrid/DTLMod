.. Copyright 2025-2026

.. _Connection_manager:

Connection Manager
==================

To use a data transport layer in your simulator, the first thing you have to do it to call :cpp:func:`DTL::create` in
the ``main`` function of your code. This will trigger the creation of the **DTLMod connection manager** and an instance
of a :ref:`Concept_DTL` object. 

The connection manager is a **daemon** SimGrid actor (i.e., an simulated process running in the background and
automatically killed when the simulation ends) that runs on the first host declared in your platform description. This
daemon actor takes the newly created :ref:`Concept_DTL` object as argument.

Once started, the connection manager creates two SimGrid message queues (i.e., a rendez-vous point between actors
thanks to which actors can exchange information without inducing any simulated cost): one to receive **connection** or
**disconnection** requests and the other to send back a handler (i.e., a shared pointer) on the :ref:`Concept_DTL` object in
answer to a connection request. 

Then, the connection manager enters an infinite loop in which it waits for connection and disconnection requests on the
first message queue. If it gets a **connection request**, meaning that an actor in your simulator called
:cpp:func:`DTL::connect`, the connection manager checks if this actor is already connected. If it is, it emits a
warning message advising you to check your code logic. Otherwise, it adds the calling actor to a list of **active
connections**, put the shared pointer on the :ref:`Concept_DTL` object in its second
message queue, and proceeds with waiting for the next request. 

Conversely, if it gets a **disconnection request**, meaning that an actor in your simulator called
:cpp:func:`DTL::disconnect`, the connection manager checks whether this actor was still connected. If it was not, it 
advises you to check your code logic. Otherwise, the connection manager removes the actor from the list of the active
connections, puts an acknowlegment to the disconnecting actor in the second message queue, and waits for the next
request.

Note that if during the execution of your simulator all your actors disconnect themselves from the DTL you will receive
a harmless message warning you that there is **no active connection** at the moment. However, the connection manager
remains active in the background and ready to process subsequent connection requests. 

If you want to ignore this message you can either add ``--log=dtlmod.t:critical`` to the command line or call 
``xbt_log_control_set("dtlmod.t:critical")`` in your code (see :ref:`Logging` for more details).
