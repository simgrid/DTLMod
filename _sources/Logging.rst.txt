.. Copyright 2025-2026

.. _Logging:

Logging and Debugging
#####################

DTLMod relies on the logging mechanisms provided by SimGrid to allow you to configure at runtime which messages to
display or omit. Each message produced in the code of DTLMod, and the simulators using it, is given a **category**
(denoting its topic) and a **priority**. Then at runtime, each category is given a **threshold** (only messages of
priority higher than that threshold are displayed), a **layout** (deciding how the messages in this category are
formatted), and an **appender** (deciding what to do with the message: either print on stderr or to a file).

For a complete description of SimGrid' logging mechanisms, look at `this page
<https://simgrid.org/doc/latest/The_XBT_toolbox.html#logging-api>`_. In this section, we just present the main
functions that you can use to instrument your DTLMod-based simulator and the different logging categories exposed by
DTLMod for debugging purposes.

Declaring categories
********************

Typically, you can define a category for each module of your simulator implementation in order to independently control
the logging for each module. Refer to the :ref:`logging_categories` section for a list of all existing categories in
DTLMod.

.. c:macro:: XBT_LOG_NEW_CATEGORY(category, description)

   Creates a new category that is not within any existing categories. ``category`` must be a valid and unique identifier
   (such as ``mycat``) and ``description`` must be a string, between quotes.

.. c:macro:: XBT_LOG_NEW_SUBCATEGORY(category, parent_category, description)

   Creates a new category under the provided ``parent_category``.

.. c:macro:: XBT_LOG_NEW_DEFAULT_CATEGORY(category, description)

   Similar to :c:macro:`XBT_LOG_NEW_CATEGORY`, and the created category is the default one in the current source file.

.. c:macro:: XBT_LOG_NEW_DEFAULT_SUBCATEGORY(category, parent_category, description)

   Similar to :c:macro:`XBT_LOG_NEW_SUBCATEGORY`, and the created category is the default one in the current source file.

.. c:macro:: XBT_LOG_EXTERNAL_CATEGORY(category)

   Make an external category (i.e., a category declared in another source file) visible from this source file.
   In each source file, at most one category can be the default one.

.. c:macro:: XBT_LOG_EXTERNAL_DEFAULT_CATEGORY(category)

   Use an external category as default category in this source file.

Logging messages
****************

.. c:macro:: XBT_CRITICAL(format_string, parameters...)

   Report a fatal error to the default category.

.. c:macro:: XBT_ERROR(format_string, parameters...)

   Report an error to the default category.

.. c:macro:: XBT_WARN(format_string, parameters...)

   Report a warning or an important information to the default category.

.. c:macro:: XBT_INFO(format_string, parameters...)

   Report an information of regular importance to the default category.

.. c:macro:: XBT_VERB(format_string, parameters...)

   Report a verbose information to the default category.

.. c:macro:: XBT_DEBUG(format_string, parameters...)

   Report a debug-only information to the default category.

For each of the logging macros, the first parameter must be a printf-like format string, and the subsequent parameters
must match this format.

Activating logging categories
*****************************

There are two ways to activate a specific logging category: on command line and within the code of your simulator.

On command line, you have to append a ``--log=`` flag to the call to your simulator. The value associated to this flag
is composed of three part: the **logging category**, followed by ``.threshold:`` (or anything subset between ``.t:``
and ``.threshol:`` to save time), and the desired **priority**.

You can also specify which format to apply to a category. For instance, `--log=mycat.fmt:%m` reduces the output to the
user-message only, removing any decoration such as the date or the actor pid. For more details on format configuration,
we refer you to the
`SimGrid documentation <https://simgrid.org/doc/latest/Configuring_SimGrid.html#format-configuration>`_.

Finally, ``--log=no_loc`` hides the source locations (file names and line numbers) from where the logs are produced.

.. code-block:: console

    $ ./my_simulator --log=mycat.threshold:debug --log=mycat.fmt:%m --log=no_loc

To configure and activate a logging category from your simulator code, use the following function (declared in
`xbt/log.h`).

.. c:function:: void xbt_log_control_set(const char* setting)

   Sets the provided ``setting`` as if it was passed in a ``--log`` command-line parameter.

.. _logging_categories:

Existing categories
*******************

Here follows the list of all the logging categories existing in the implementation of DTLMod.

- dtlmod

  - dtl
  - dtlmod_stream
  - dtlmod_engine

   - dtlmod_file_engine
   - dtlmod_staging_engine
  
  - dtlmod_transport

   - dtlmod_file_transport
   - dtlmod_staging_transport 

  - dtlmod_variable

  - dtlmod_metadata
