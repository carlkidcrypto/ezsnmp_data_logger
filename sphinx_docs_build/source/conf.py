# Configuration file for the Sphinx documentation builder.

import os
import sys

sys.path.insert(0, os.path.abspath("../.."))

# -- Project information -----------------------------------------------------

project = "EzSnmp Data Logger"
copyright = "2024-%Y, carlkidcrypto"
author = "carlkidcrypto"
release = "v2026.11.5.1"

# -- General configuration ---------------------------------------------------

extensions = [
    "sphinx.ext.autodoc",
    "sphinx.ext.viewcode",
    "sphinx.ext.githubpages",
    "breathe",
]

breathe_projects = {"EzSnmpDataLogger": "../../doxygen_docs_build/doxygen/xml"}
breathe_default_project = "EzSnmpDataLogger"

templates_path = ["_templates"]
exclude_patterns = []

# -- Options for HTML output -------------------------------------------------

html_theme = "furo"
html_baseurl = "https://carlkidcrypto.github.io/ezsnmp_data_logger/html/"
html_title = "EzSnmp Data Logger Documentation"
html_static_path = ["_static"]
