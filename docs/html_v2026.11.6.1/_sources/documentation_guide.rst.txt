====================
Documentation Guide
====================

Overview
========

EzSnmp Data Logger mirrors the exact documentation architecture used across `ezsnmp <https://github.com/carlkidcrypto/ezsnmp>`_, ``purpleair_data_logger``, and ``purpleair_api``:

1. **Doxygen** extracts XML API metadata from C++17 headers in ``include/ezsnmp_data_logger/`` and ``src/``.
2. **Sphinx + Breathe + Furo** builds the main HTML documentation from ``sphinx_docs_build/source/`` into ``docs/html/``.
3. **Version-Selector Landing Page** builds ``sphinx_docs_build/landing/source/index.rst`` into ``docs/index.html``, linking ``Latest Alpha/Beta Version`` (``./html/index.html``) alongside historical release snapshots (``./html_vX.Y.Z/index.html``).

Building Documentation Locally
==============================

1. Generate Doxygen XML
-----------------------

Doxygen must run **before** Sphinx so Breathe can read ``doxygen_docs_build/doxygen/xml/``:

.. code-block:: bash

   cd /path/to/ezsnmp_data_logger
   rm -rf doxygen_docs_build
   mkdir -p doxygen_docs_build
   doxygen .doxygen

2. Build Main Sphinx Documentation
----------------------------------

Install documentation dependencies and build with ``SPHINXOPTS="-W"`` (warnings treated as errors):

.. code-block:: bash

   python3 -m pip install -r sphinx_docs_build/requirements.txt
   python3 setup.py build_ext --inplace
   cd sphinx_docs_build
   mkdir -p source/_static source/_templates
   make clean
   make html SPHINXOPTS="-W"

The generated site is written to ``docs/html/index.html``.

3. Build Version-Selector Landing Page
--------------------------------------

.. code-block:: bash

   cd sphinx_docs_build/landing
   mkdir -p source/_static source/_templates
   make html SPHINXOPTS="-W"

The top-level landing page is written to ``docs/index.html``.

.. note::
   Current build outputs (``docs/html/``, ``docs/_static/``, ``docs/doctrees/``, ``doxygen_docs_build/``) are ignored by ``.gitignore`` and deployed automatically to GitHub Pages by ``.github/workflows/sphinx_build.yml``. Versioned release folders (``docs/html_v*``) are preserved on ``main`` when tags are published.

Documentation Standards
=======================

* **Python Docstrings:** Use Sphinx/reStructuredText field lists (``:param:``, ``:return:``) on public classes and functions.
* **C++17 Comments:** Use Doxygen ``/** ... */`` blocks with ``@brief``, ``@param``, ``@return``, and ``@throws`` tags on all public declarations in ``include/ezsnmp_data_logger/``.
* **Software Requirements:** Maintain ``[PREFIX-nn]`` + ``Shall`` requirements in :doc:`software_requirements`.
