.. Copyright 2025
.. DTLMod documentation master file

.. _index:

DTLMod: The SimGrid Data Transport Layer Module
===============================================

In situ processing does not only allow scientific applications to face the explosion in data volume and velocity but
also provides scientists with early insights about their applications at runtime. Multiple frameworks implement the
concept of a **Data transport Layer (DTL)** to enable such in situ workflows. These tools are very versatile, directly
or indirectly access the data generated on the same node, another node of the same compute cluster, or a completely
distinct node, and allow data publishers and subscribers to run on the same computing resources or not.

This versatility puts on researchers the onus of taking key decisions related to resource allocation and how to
transport data to ensure the most efficient execution of their in situ workflows. However, domain scientists and
workflow practitioners lack the appropriate tools to assess the respective performance of particular design and
deployment options. 

DTLMod is a **versatile simulated DTL** designed to provide researchers with insights on the respective performance of
different execution scenarios of in situ workflows. This open-source, standalone library builds on the
`SimGrid toolkit <https://simgrid.org/>`_ and can be linked to any SimGrid-based simulator. It facilitates the
evaluation of the performance behavior, at scale, of different data transport configurations and the study of the
effects of resource allocation strategies. 

.. toctree::
   :hidden:
   :maxdepth: 2
   :caption: User Manual:

      Introduction <Introduction.rst>
         Installing DTLMod <Installing_DTLMod.rst>
         Start your own project <New_project.rst>
      Describing your in situ workflow <Workflow.rst>
         The DTLMod programming interface <app_API.rst>
