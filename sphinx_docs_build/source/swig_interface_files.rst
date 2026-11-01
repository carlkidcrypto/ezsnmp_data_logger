=======================
SWIG Interface File(s)
=======================

The following modular SWIG 4.x interface files in ``interface/`` wrap the C++17 ``ezsnmp_data_logger`` engine for the 4 supported target languages: Python, JavaScript (Node.js N-API), C# (.NET P/Invoke), and Go (cgo).

For more information on SWIG interface syntax, see the `SWIG 4 Documentation <https://www.swig.org/Doc4.3/SWIGDocumentation.html>`_.

Top-Level Module (``ezsnmp_data_logger.i``)
===========================================

.. literalinclude:: ../../interface/ezsnmp_data_logger.i
   :language: c

Exception Mapping (``exceptions.i``)
====================================

.. literalinclude:: ../../interface/exceptions.i
   :language: c

Telemetry Datatypes (``datatypes.i``)
=====================================

.. literalinclude:: ../../interface/datatypes.i
   :language: c

JSON Configuration & Validation (``config.i``)
==============================================

.. literalinclude:: ../../interface/config.i
   :language: c

DataLogger Engine & CLI Runner (``data_logger.i``)
==================================================

.. literalinclude:: ../../interface/data_logger.i
   :language: c
