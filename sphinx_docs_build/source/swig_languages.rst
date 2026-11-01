================================
SWIG Multi-Language Distribution
================================

**EzSnmp Data Logger** is built from day one so that a single **C++17** backend (powered by ``ezsnmp``'s ``SessionBase`` and ``Result`` layers) can be distributed to every language target supported by **SWIG 4.x**.

Supported SWIG Target Languages
===============================

The unified generator script ``tools/generate_swig_bindings.py`` validates and emits bindings for **16 SWIG 4.x targets**:

.. list-table::
   :header-rows: 1
   :widths: 18 22 60

   * - Language
     - SWIG Invocation
     - Build Scaffold / Usage
   * - **Python**
     - ``swig -c++ -python``
     - ``pyproject.toml`` + ``setup.py`` (``pip install .``)
   * - **JavaScript (Node.js)**
     - ``swig -c++ -javascript -napi``
     - ``bindings/javascript/package.json`` + ``binding.gyp`` (``node-gyp``)
   * - **Java (JNI)**
     - ``swig -c++ -java -package io.github.carlkidcrypto.ezsnmp_data_logger``
     - ``bindings/java/CMakeLists.txt``
   * - **C# (.NET P/Invoke)**
     - ``swig -c++ -csharp -namespace EzSnmpDataLogger``
     - ``bindings/csharp/EzSnmpDataLogger.csproj``
   * - **Go (cgo)**
     - ``swig -c++ -go -cgo -intgosize 64``
     - ``bindings/go/go.mod`` + ``ezsnmp_data_logger_cgo.go``
   * - **Ruby**
     - ``swig -c++ -ruby``
     - ``bindings/ruby/extconf.rb`` (``mkmf``)
   * - **Perl 5**
     - ``swig -c++ -perl5``
     - ``bindings/perl5/Makefile.PL`` (``ExtUtils::MakeMaker``)
   * - **PHP 8+**
     - ``swig -c++ -php``
     - Generated ``php_ezsnmp_data_logger.h`` + C++ wrapper
   * - **Lua**
     - ``swig -c++ -lua``
     - Generated Lua C++ module wrapper
   * - **R**
     - ``swig -c++ -r``
     - ``bindings/r/src/Makevars`` (``R CMD SHLIB``)
   * - **Tcl 8**
     - ``swig -c++ -tcl8``
     - Generated Tcl package C++ wrapper
   * - **GNU Octave**
     - ``swig -c++ -octave``
     - Build with ``mkoctfile``
   * - **D**
     - ``swig -c++ -d``
     - Generated D module + C++ wrapper
   * - **GNU Guile**
     - ``swig -c++ -guile``
     - Generated Guile C++ wrapper
   * - **Scilab**
     - ``swig -c++ -scilab``
     - Generated Scilab gateway C++ wrapper
   * - **XML AST**
     - ``swig -c++ -xml``
     - Full machine-readable SWIG parse tree for custom FFI generators (Rust, Zig, Swift, Kotlin/Native)

Generating Bindings
===================

Generate all 16 language targets into ``generated_swig/<lang>/``:

.. code-block:: bash

   python3 tools/generate_swig_bindings.py --out-dir generated_swig

Or generate a subset of languages directly into ``bindings/<lang>/generated/`` for compilation with each language's native toolchain:

.. code-block:: bash

   python3 tools/generate_swig_bindings.py --languages python javascript java csharp go ruby --in-bindings

Cross-Language Idiomatic Usage
==============================

Because all settings live in **JSON** and every core type exposes ``to_json()`` / ``from_json()`` alongside typed struct accessors, every target language gets an ergonomic, zero-boilerplate API:

Python
------

.. code-block:: python

   import ezsnmp_data_logger as edl

   dl = edl.DataLogger.from_file("configs/example.json")
   batches = dl.run_once()
   for batch in batches:
       print(batch.dut_name, len(batch.samples), batch.to_json())

Node.js (N-API)
---------------

.. code-block:: javascript

   const edl = require('./build/Release/ezsnmp_data_logger');

   const dl = edl.DataLogger.from_file('configs/example.json');
   const batches = dl.run_once();
   console.log(dl.stats_json(2));

Go (cgo)
--------

.. code-block:: go

   package main

   import (
       "fmt"
       edl "github.com/carlkidcrypto/ezsnmp_data_logger/bindings/go"
   )

   func main() {
       dl := edl.DataLoggerFrom_file("configs/example.json")
       defer edl.DeleteDataLogger(dl)
       dl.Run_once()
       fmt.Println(dl.Stats_json(2))
   }

Java (JNI)
----------

.. code-block:: java

   import io.github.carlkidcrypto.ezsnmp_data_logger.*;

   public class Example {
       public static void main(String[] args) {
           DataLogger dl = DataLogger.from_file("configs/example.json");
           BatchVector batches = dl.run_once();
           System.out.println(dl.stats_json(2));
       }
   }

C# (.NET)
---------

.. code-block:: csharp

   using System;
   using EzSnmpDataLogger;

   using var dl = DataLogger.from_file("configs/example.json");
   var batches = dl.run_once();
   Console.WriteLine(dl.stats_json(2));
