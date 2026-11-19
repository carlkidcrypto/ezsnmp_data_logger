===================
System Architecture
===================

Layered Design
==============

``ezsnmp_data_logger`` cleanly separates SNMP protocol transport, concurrent queuing, persistent storage sinks, multi-language SWIG bindings, and user interfaces:

.. code-block:: text

   +-----------------------------------------------------------------------+
   |     Cross-Platform GUIs (Linux / macOS / Windows)                     |
   |  [Native tkinter.ttk Desktop Studio]   [Browser Web Studio (HTTP)]    |
   +-----------------------------------+-----------------------------------+
                                       |  subprocess + JSON (--json)
                                       v
   +-----------------------------------------------------------------------+
   |          CLI Layer (`ezsnmp-data-logger` / `CliRunner`)               |
   |   run | validate | probe | request | set | inspect | init-config      |
   +-----------------------------------+-----------------------------------+
                                       |
   +-----------------------------------v-----------------------------------+
   |      SWIG 4.x Universal C++17 API (`DataLogger` PIMPL Boundary)       |
   |  Python | Node N-API | Java | C# | Go | Ruby | Perl | PHP | Lua | R   |
   +-----------------+-----------------------------------+-----------------+
                     |                                   |
                     v                                   v
   +----------------------------------+   +--------------------------------+
   |   SNMP Polling Pool (C++17)      |   |  Sink Writer Thread (C++17)    |
   |  - EzSnmpPoller -> ::SessionBase |-->|  BoundedQueue<Batch>           |
   |    (get/walk/bulkwalk/set)       |   |    -> SqliteSink (WAL)         |
   |  - StubSnmpPoller (synthetic)    |   |    -> CsvSink / JsonlSink      |
   +----------------------------------+   |    -> PsqlSink (TimescaleDB)   |
                                          |    -> PrometheusSink (/metrics)|
                                          +--------------------------------+

EzSnmp ``SessionBase`` Integration
==================================

Rather than re-implementing Net-SNMP session or PDU handling, ``EzSnmpPoller`` instantiates ``::SessionBase`` directly from the sibling ``ezsnmp`` C++17 codebase:

- **Scalar Coalescing:** All ``"operation": "get"`` metrics configured on a DUT are coalesced into a single multi-OID ``SessionBase::get(std::vector<std::string>)`` request per polling cycle.
- **Bulk & Tree Traversals:** ``getnext``, ``walk``, ``bulkget``, and ``bulkwalk`` metrics invoke ``SessionBase::get_next``, ``walk``, ``bulk_get``, and ``bulk_walk``, with an RAII ``ScopedMaxRepeaters`` guard applying ``-Cr<N>`` exclusively during ``GETBULK`` PDUs.
- **SNMP SET Support:** Both scheduled metrics and on-demand CLI/GUI ``set`` operations invoke ``SessionBase::set({oid, set_type, set_value})``.
- **Lossless Metadata Storage:** Every ``ezsnmp::Result`` preserves ``oid``, ``index`` (``oid_index``), ``type`` (``snmp_type``), and ``value`` (``raw_value``) alongside strongly typed ``i64``, ``f64``, and ``str_val`` columns extracted via ``std::get_if`` on ``Result::converted_value``.
