.. Copyright 2025

.. _Engines:

Engines
=======

The set of functions in the DTLMod API that are related to engines is limited to the following ones:

  - :cpp:func:`begin_transaction`
  - :cpp:func:`put` or :cpp:func:`get`
  - :cpp:func:`end_transaction`
  - :cpp:func:`close`

However, this simple API hides more complex behaviors that depend on the engine type (i.e., File or Staging) and of
whether these functions are called on the publisher or subscriber side. In this section, we describe the internal 
behavior of the File and Staging engines.

