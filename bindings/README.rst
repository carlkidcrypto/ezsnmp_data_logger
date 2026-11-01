==============================================
Multi-Language SWIG Bindings & Packaging Guide
==============================================

``ezsnmp_data_logger`` exposes its entire C++17 telemetry engine, JSON validator,
``ezsnmp`` SNMP request/set bridge, and CLI runner through universal SWIG 4.x
interface files in ``interface/``.

Generating Bindings for All 16 SWIG Targets
===========================================

Run the binding generator from the repository root:

.. code-block:: bash

   python3 tools/generate_swig_bindings.py --languages all

Supported SWIG targets:

- **Python** (``-python``, distributed via ``pyproject.toml`` / ``setup.py``)
- **JavaScript / Node.js** (``-javascript -napi``, scaffold in ``bindings/javascript/``)
- **Java** (``-java``, JNI scaffold in ``bindings/java/``)
- **C# / .NET** (``-csharp``, P/Invoke scaffold in ``bindings/csharp/``)
- **Go** (``-go -cgo -intgosize 64``, module scaffold in ``bindings/go/``)
- **Ruby** (``-ruby``, ``mkmf`` scaffold in ``bindings/ruby/``)
- **Perl 5** (``-perl5``, MakeMaker scaffold in ``bindings/perl5/``)
- **PHP** (``-php``)
- **Lua** (``-lua``)
- **R** (``-r``, Makevars scaffold in ``bindings/r/``)
- **Tcl 8** (``-tcl8``)
- **GNU Octave** (``-octave``)
- **D** (``-d -d2``)
- **GNU Guile** (``-guile``)
- **Scilab** (``-scilab``)
- **SWIG XML AST** (``-xml``)
