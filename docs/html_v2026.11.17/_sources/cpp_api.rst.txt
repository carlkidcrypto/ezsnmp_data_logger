===================
C++17 API Reference
===================

This page documents the portable ISO C++17 classes, structs, enums, and functions in namespace ``ezsnmp_data_logger``, generated automatically from header Doxygen comments via Breathe.

Core Engine & CLI Dispatcher
============================

DataLogger
----------

.. doxygenclass:: ezsnmp_data_logger::DataLogger
   :members:

CliRunner & CliResult
---------------------

.. doxygenclass:: ezsnmp_data_logger::CliRunner
   :members:

.. doxygenstruct:: ezsnmp_data_logger::CliResult
   :members:

EzSnmp Polling Layer
====================

``EzSnmpPoller`` delegates all SNMPv1, SNMPv2c, and SNMPv3 ``get``, ``getnext``, ``walk``, ``bulkget``, ``bulkwalk``, and ``set`` requests to ``ezsnmp``'s C++17 ``SessionBase`` and ``Result`` classes, normalizing returned bindings into ``Sample`` and ``Batch`` records.

.. doxygenclass:: ezsnmp_data_logger::ISnmpPoller
   :members:

.. doxygenclass:: ezsnmp_data_logger::EzSnmpPoller
   :members:

.. doxygenclass:: ezsnmp_data_logger::StubSnmpPoller
   :members:

.. doxygenfunction:: ezsnmp_data_logger::create_snmp_poller

JSON Configuration Model
========================

.. doxygenstruct:: ezsnmp_data_logger::LoggerConfig
   :members:

.. doxygenstruct:: ezsnmp_data_logger::LoggerRuntimeConfig
   :members:

.. doxygenstruct:: ezsnmp_data_logger::DutConfig
   :members:

.. doxygenstruct:: ezsnmp_data_logger::SnmpConfig
   :members:

.. doxygenstruct:: ezsnmp_data_logger::MetricConfig
   :members:

.. doxygenstruct:: ezsnmp_data_logger::SinkConfig
   :members:

.. doxygenstruct:: ezsnmp_data_logger::GuiConfig
   :members:

.. doxygenstruct:: ezsnmp_data_logger::ValidationReport
   :members:

Telemetry Datatypes
===================

.. doxygenenum:: ezsnmp_data_logger::TypeTag

.. doxygenstruct:: ezsnmp_data_logger::Sample
   :members:

.. doxygenstruct:: ezsnmp_data_logger::Batch
   :members:

.. doxygenstruct:: ezsnmp_data_logger::DutProbeResult
   :members:

.. doxygenstruct:: ezsnmp_data_logger::LoggerStats
   :members:

.. doxygenstruct:: ezsnmp_data_logger::InspectRow
   :members:

Storage Sinks & Inspection
==========================

.. doxygenclass:: ezsnmp_data_logger::ISink
   :members:

.. doxygenfunction:: ezsnmp_data_logger::create_sink

.. doxygenfunction:: ezsnmp_data_logger::inspect_configured_sinks

Exceptions
==========

.. doxygenclass:: ezsnmp_data_logger::LoggerError
   :members:

.. doxygenclass:: ezsnmp_data_logger::ConfigError
   :members:

.. doxygenclass:: ezsnmp_data_logger::SnmpError
   :members:

.. doxygenclass:: ezsnmp_data_logger::SinkError
   :members:

.. doxygenclass:: ezsnmp_data_logger::CliError
   :members:
