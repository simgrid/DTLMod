.. Copyright 2025-2026

.. _Compression:

Compression
###########

Lossy compression is a widely used data reduction technique in scientific computing. By accepting a controlled loss
of precision, compressors such as SZ and ZFP can achieve significant reductions in data volume while preserving the
features that matter for downstream analysis. DTLMod models the performance impact of compression on in situ
workflows without actually compressing any data: it simulates the computational cost of compression and
decompression and adjusts the volume of data transported through the DTL according to a compression ratio.

How compression works in DTLMod
-------------------------------

Unlike decimation, compression does not change the **shape** of a variable. A :math:`1000 \times 1000` array remains
a :math:`1000 \times 1000` array after compression. What changes is the **byte-size** of the variable: the number
of bytes transported through the DTL is divided by the compression ratio. This reflects the fact that real-world
lossy compressors produce a bitstream that is smaller than the original data but still represents all the elements
of the array.

Compression is a **publisher-side only** operation. Applying compression on the subscriber side is not meaningful
because compression aims to reduce the volume of data that needs to be transported---which requires intervention
before the data leaves the publisher.

Compressor profiles
-------------------

DTLMod provides three ways to determine the compression ratio for a variable:

**Fixed ratio.** The simplest option: you directly specify the desired compression ratio. This is useful when you
already know, from experiments or from the literature, the compression ratio achieved by a particular compressor
on data similar to yours. The ratio must be at least 1.0 (a ratio of 1 means no size reduction).

**SZ profile.** This profile is inspired by the `SZ lossy compressor <https://szcompressor.org/>`_, a
prediction-based algorithm. SZ achieves high compression ratios on smooth scientific data because it can accurately
predict neighboring values and only store the (small) prediction errors. The compression ratio is derived from two
user-specified parameters:

- **accuracy** (or error bound): the maximum acceptable pointwise error. Tighter accuracy requirements reduce the
  compression ratio because more bits are needed to represent the prediction residuals.

- **data smoothness**: a value between 0 and 1 that characterizes how regular the data is. Smooth data
  (e.g., temperature fields) yields higher compression ratios because predictions are more accurate. Noisy or
  turbulent data yields lower ratios.

The model computes the ratio as:

.. math::

   r = \max\!\Big(1,\;\alpha \cdot \left(-\log_{10} \varepsilon\right)^{\beta} \cdot (0.5 + \sigma)\Big)

where :math:`\varepsilon` is the accuracy, :math:`\sigma` is the data smoothness, and :math:`\alpha = 3.0`,
:math:`\beta = 0.8` are empirical parameters fitted from published benchmarks on scientific datasets.

**ZFP profile.** This profile is inspired by the `ZFP compressor <https://computing.llnl.gov/projects/zfp>`_,
a transform-based algorithm. ZFP organizes data into small blocks, applies a near-orthogonal transform, and
encodes the resulting coefficients with a fixed number of bits per value. The compression ratio depends primarily
on the requested accuracy:

.. math::

   \text{rate} = \max(1,\;-\log_2 \varepsilon + 1) \quad;\quad r = \frac{64}{\text{rate}}

where the rate represents the number of bits per double-precision value after compression. Higher accuracy
requirements increase the rate and therefore decrease the compression ratio.

Compression and decompression costs
------------------------------------

Two independent cost parameters control the simulated computational overhead of compression:

- **compression cost per element**: the number of floating-point operations incurred per array element when
  compressing the data on the publisher side.

- **decompression cost per element**: the number of floating-point operations incurred per array element when
  decompressing the data on the subscriber side, after it has been received.

Both parameters default to 1.0. The total compression cost for a variable is computed as:

.. math::

   C_{\text{compress}} = c_{\text{comp}} \times \frac{N_{\text{local}}}{\text{element\_size}}

.. math::

   C_{\text{decompress}} = c_{\text{decomp}} \times \frac{N_{\text{local}}}{\text{element\_size}}

where :math:`N_{\text{local}}` is the local size of the variable in bytes and :math:`\text{element\_size}` is the
size of one array element. The compression cost is incurred by the publisher right before putting the variable into
the DTL, and the decompression cost is incurred by the subscriber right after receiving it.

Per-transaction variability
---------------------------

In practice, the compression ratio achieved on a given variable varies from one time step to the next as the data
evolves. DTLMod can model this variability through an optional **ratio variability** parameter that introduces a
bounded, deterministic perturbation around the nominal compression ratio at each transaction. This enables the
simulation of realistic scenarios in which the effectiveness of compression fluctuates over the course of a run.

Re-parameterization
-------------------

As with decimation, compression parameters can be updated between transactions. You can change the compression
ratio, switch compressor profiles, adjust accuracy or smoothness, or modify the cost parameters for a variable that
is already being compressed. Only the parameters that are explicitly provided in the update are modified; the
others retain their previous values. This supports the simulation of adaptive compression strategies that adjust
their settings in response to changes in the data.
