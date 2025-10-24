.. Copyright 2025

.. _DTLMod_doc:

The DTLMod programming interface
################################

.. image:: img/DTL-overview.png
   :width: 80% 
   :align: center

.. raw:: html

   <br><br>

.. _DTLMod_main_concepts:

Main Concepts
*************

DTL
^^^
.. |Concept_DTL| replace:: **DTL**
.. _Concept_DTL:

TBD

Streams
^^^^^^^
.. |Concept_Stream| replace:: **Stream**
.. _Concept_Stream:

TBD

Engines
^^^^^^^
.. |Concept_Engine| replace:: **Engine**
.. _Concept_Engine:

TBD

Variables
^^^^^^^^^^
.. |Concept_Variable| replace:: **Variable**
.. _Concept_Variable:

At the core of the DTLMod is the data transported from publishers to subscribers. Many in situ processing workflows
involve parallel MPI codes as data producers. These codes manipulate **multidimensional arrays** distributed over
multiple ranks. DTLMod adopts this data structure as the basis of its |Concept_Variable|_ abstraction.

The figure below illustrates shows how to define a 3-dimensional array, distributed to eight MPI ranks organized in 
a :math:`2 \times 2 \times 2` grid, as a **self-descriptive tuple**.

.. image:: img/3D-variable.png
   :width: 60%
   :align: center

.. raw:: html

   <br><br>

This tuple stores the **name** of the variable (that is unique to a given |Concept_Stream|_), the **global dimensions**
of the multidimensional array (:math:`G_x`, :math:`G_y`, and :math:`G_z`) and, for each rank, the **local part**
(:math:`L_x`, :math:`L_y`, and :math:`L_z`) owned by that rank after decomposition and distribution, and a
**3D-offset** (represented by a star in the figure) that indicates where the local array is positioned in the global
array. Finally, the tuple stores the **size of the elements** in the array.

Transactions
^^^^^^^^^^^^
.. |Concept_Transaction| replace:: **Transaction**
.. _Concept_Transaction:

TBD


API Reference
*************

.. _API_dtlmod_DTL:


class DTL
^^^^^^^^^

A class that implements a Data Transport Layer abstraction.

Creation
---------

.. tabs::

   .. group-tab:: C++

      .. code:: C++

         #include <dtlmod/DTL.hpp>

      .. doxygenfunction:: dtlmod::DTL::create()   
      .. doxygenfunction:: dtlmod::DTL::create(const std::string& config_filename)

   .. group-tab:: Python

      .. code:: Python

         import dtlmod

      .. automethod:: dtlmod.DTL.create   

Connection and Deconnection
---------------------------
.. tabs::

   .. group-tab:: C++

      .. doxygenfunction:: dtlmod::DTL::connect()
      .. doxygenfunction:: dtlmod::DTL::disconnect()
      .. doxygenfunction:: dtlmod::DTL::has_active_connections

   .. group-tab:: Python

      .. automethod:: dtlmod.DTL.connect
      .. automethod:: dtlmod.DTL.disconnect
      .. autoproperty:: dtlmod.DTL.has_active_connections

Stream factory
--------------
.. tabs::

   .. group-tab:: C++

      .. doxygenfunction:: dtlmod::DTL::add_stream(const std::string& name)
      .. doxygenfunction:: dtlmod::DTL::get_stream_by_name_or_null(const std::string& name) const
      .. doxygenfunction:: dtlmod::DTL::get_all_streams

   .. group-tab:: Python

      .. automethod:: dtlmod.DTL.add_stream
      .. automethod:: dtlmod.DTL.stream_by_name_or_null
      .. autoproperty:: dtlmod.DTL.all_streams

class Engine
^^^^^^^^^^^^

TBD

class Stream
^^^^^^^^^^^^

TBD

class Variable
^^^^^^^^^^^^^^

TBD
