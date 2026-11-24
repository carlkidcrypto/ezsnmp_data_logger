====================
Python API Reference
====================

This page documents the Python package ``ezsnmp_data_logger``, its CLI entry point, and the cross-platform GUI modules (``CliSubprocessBridge`` and ``StudioHttpServer``) that drive the CLI behind the scenes across Linux, macOS, and Windows.

Top-Level Package (``ezsnmp_data_logger``)
==========================================

.. automodule:: ezsnmp_data_logger
   :members: DataLogger, CliRunner, CliResult, LoggerConfig, LoggerRuntimeConfig, DutConfig, SnmpConfig, MetricConfig, SinkConfig, GuiConfig, ValidationReport, Sample, Batch, DutProbeResult, LoggerStats, InspectRow
   :undoc-members:

CLI Module (``ezsnmp_data_logger.cli``)
=======================================

.. automodule:: ezsnmp_data_logger.cli
   :members:
   :undoc-members:

GUI Launcher (``ezsnmp_data_logger.gui``)
=========================================

.. automodule:: ezsnmp_data_logger.gui
   :members:
   :undoc-members:

GUI Behind-the-Scenes CLI Bridge (``ezsnmp_data_logger.gui.cli_bridge``)
========================================================================

.. automodule:: ezsnmp_data_logger.gui.cli_bridge
   :members:
   :undoc-members:

GUI Cross-Platform Browser Studio Server (``ezsnmp_data_logger.gui.server``)
============================================================================

.. automodule:: ezsnmp_data_logger.gui.server
   :members:
   :undoc-members:

GUI Cross-Platform Native Desktop Studio (``ezsnmp_data_logger.gui.desktop_tk``)
================================================================================

.. automodule:: ezsnmp_data_logger.gui.desktop_tk
   :members:
   :undoc-members:
