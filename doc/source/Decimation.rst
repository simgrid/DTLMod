.. Copyright 2025-2026

.. _Decimation:

Decimation
##########

Decimation is a spatial subsampling technique that reduces the size of a multidimensional array by keeping only every
*n*-th element along each dimension. It is the method of choice when a workflow component does not need the full
resolution of the data produced upstream---a common situation in visualization or coarse-grained analysis pipelines.

How decimation works
--------------------

A decimation operation is controlled by a **stride vector** that specifies, for each dimension of a
:ref:`Concept_Variable`, how many elements to skip between two retained samples. For a variable of shape
:math:`(D_1, D_2, \ldots, D_k)` and a stride :math:`(s_1, s_2, \ldots, s_k)`, the shape of the reduced variable
becomes :math:`(\lceil D_1/s_1 \rceil, \lceil D_2/s_2 \rceil, \ldots, \lceil D_k/s_k \rceil)`.

For instance, applying a stride of :math:`(1, 2, 4)` to a :math:`640 \times 640 \times 640` variable produces a
:math:`640 \times 320 \times 160` reduced variable---an 8x reduction in data volume.

The stride vector must have the same number of dimensions as the variable it applies to and all stride values must
be strictly positive. A stride of 1 along a given dimension means no subsampling in that dimension.

Decimation is applied **per variable**: within the same :ref:`Concept_Stream`, different variables can be decimated
with different strides, or not be decimated at all.

Publisher-side and subscriber-side decimation
---------------------------------------------

Unlike compression, decimation can be applied on **both sides** of the data flow:

- When applied by a **publisher**, decimation reduces the volume of data that leaves the publisher. The simulated
  cost of the decimation kernel is incurred before the data is transported. Only the decimated version of the variable
  is put into the DTL, which directly reduces I/O or network costs.

- When applied by a **subscriber**, decimation reduces the volume of data that the subscriber has to process before 
  retrieving it from the DTL. 

Interpolation
-------------

In some workflows, the subsampled data must be smoothed or reconstructed to better approximate the original field.
DTLMod models this by allowing an optional **interpolation method** to be specified alongside the stride. The
supported interpolation methods are:

- **linear**: suitable for piecewise-linear fields (variables with at least 1 dimension).
- **quadratic**: suitable for smoother fields (variables with at least 2 dimensions).
- **cubic**: suitable for highly smooth fields (variables with at least 3 dimensions).

The choice of interpolation method does not affect the size of the reduced variable: it only affects the
**computational cost** of the decimation operation. Higher-order interpolation is more expensive: the cost
multiplier is 2x for linear, 4x for quadratic, and 8x for cubic interpolation relative to simple subsampling
without interpolation. This allows you to study the tradeoff between the quality of the reconstructed data and the
computational overhead introduced by the interpolation step.

Computational cost model
------------------------

The simulated cost of a decimation operation, in floating-point operations, is determined by:

.. math::

   C = m \times c \times N

where :math:`N` is the number of elements in the **local** (non-decimated) portion of the variable,
:math:`c` is a configurable **cost per element** (defaulting to 1.0), and :math:`m` is the interpolation
multiplier (1 for no interpolation, 2 for linear, 4 for quadratic, 8 for cubic).

The cost per element can be adjusted to match the observed or estimated computational cost of a specific decimation
implementation in the real-world application being simulated.

Re-parameterization
-------------------

A decimation operation can be re-parameterized between transactions. For instance, you can change the stride, the
interpolation method, or the cost per element of a variable that has already been decimated. This enables the
simulation of adaptive workflows in which the level of subsampling changes over time in response to features detected
in the data.

When re-parameterizing, only the parameters that are explicitly provided are updated; the others retain their
previous values.
