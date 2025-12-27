.. Copyright 2025-2026

.. _Inside_staging_engine:

Inside the Staging engine
#########################

The **Staging Engine** implements memory-to-memory and communication-based data movement for the **DTL**. It
complements the **File Engine** by transporting Variables directly from publishers to subscribers without having to
read or write files on a shared filesystem. This type of Engine requires publishers and subscribers to be connected to
a Stream at the same time when performing a transaction. The interactions with the DTL are thus **synchronized**.

When streaming data, a :math:`M \times N` data redistribution among *M* publishers and *N* subscribers may be
necessary. The exact redistribution pattern is automatically determined by DTLMod in three steps:

  1. When a publisher **puts** a variable into a stream, it asynchronously waits for data requests (using
     zero-simulated-cost message queues) from any subscriber that opened that stream;
  2. When a subscriber **gets** (a subset of) this variable from the stream, it computes which publishers own pieces
     of its local view of the variable and send them each a request to put the corresponding pieces, defined by offsets
     and element counts, in dedicated mailboxes (resp. message queues);
  3. When publishers end their transaction, they asynchronously put the requested pieces in these mailboxes (resp.
     message queues). DTLMod then simulates the corresponding data exchanges, and may possibly force actors to wait for
     their completion when a new transaction starts.
