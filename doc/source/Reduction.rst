.. Copyright 2025-2026

.. _Reduction:

Data Reduction Operations
=========================

Scientific simulations and in situ processing workflows produce ever-increasing volumes of data. Even with fast
networks and storage systems, the sheer amount of data transported through the DTL can become a bottleneck.
**Data reduction** techniques alleviate this pressure by decreasing the volume of data that must be moved between
publishers and subscribers, at the cost of some additional computation and, depending on the method, a controlled
loss of information.

DTLMod allows you to study the impact of data reduction on the performance of in situ workflows by attaching a
**reduction method** to a :ref:`Concept_Stream` and then applying it, with specific parameters, to individual
:ref:`Concept_Variable` objects. When a publisher puts a reduced variable into the DTL, the simulation accounts for
the computational cost of the reduction operation and transports a smaller volume of data. On the subscriber side,
the simulation may account for a corresponding decompression or reconstruction cost when retrieving the variable.

DTLMod currently exposes two families of reduction methods:

**Decimation** selectively retains a subset of elements from a multidimensional array by applying a per-dimension
stride. The result is a smaller array whose shape reflects the subsampling factor in each dimension. This approach
is common in visualization pipelines where only every *n*-th data point is needed. Since decimation preserves the
original values of the retained elements, it is by nature a lossless operation on the selected subset. Optionally,
an interpolation step can be used to reconstruct missing values. More details are given in the
:ref:`Decimation` section.

**Compression** reduces the byte-size of a variable without altering its shape. The compressed variable retains the
same number of elements but each element occupies fewer bytes, according to a **compression ratio** that can be
specified directly or derived from a compressor model. DTLMod provides built-in models inspired by the SZ and ZFP
lossy compressors to derive realistic compression ratios from data characteristics. More details are given in the
:ref:`Compression` section.

Where and when reduction is applied
------------------------------------

Reduction methods can be applied on either side of the data flow:

- **Publisher-side reduction** is the most common scenario. The publisher compresses or decimates data before putting
  it into the DTL, reducing the volume of data that has to be transported and stored. Both decimation and compression
  support this mode.

- **Subscriber-side reduction** is only available for decimation. A subscriber can choose to retrieve a subsampled
  version of a variable it receives from the DTL, reducing the volume of data it has to process. Compression on the
  subscriber side is not supported because its purpose is precisely to reduce what has to be transported, which
  requires intervention before the data leaves the publisher.

When a publisher applies a reduction, the information is propagated to subscribers: any variable obtained through
``inquire_variable`` on the subscriber side carries the reduction state set by the publisher. This allows DTLMod to
prevent conflicting reduction operations. In particular, a subscriber cannot apply a second reduction to a variable
that was already reduced by its publisher.

Simulated costs
---------------

A reduction operation in DTLMod introduces two potential costs:

1. A **reduction cost** (in simulated floating-point operations) is incurred by the actor that applies the reduction,
   right before it puts or gets the variable. This cost models the computational overhead of running a compressor or
   a decimation kernel.

2. A **decompression cost** (for compression only) is incurred on the subscriber side after it receives compressed
   data. This cost models the time needed to decompress the data before it can be used by the analysis component.

These costs are fully configurable through the parameters of each reduction method, enabling you to explore tradeoffs
between data movement savings and computational overhead for different reduction strategies.
