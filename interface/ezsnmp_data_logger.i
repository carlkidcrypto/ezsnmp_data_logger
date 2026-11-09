%module ezsnmp_data_logger

#ifdef SWIGPYTHON
%feature("autodoc", "2");
%feature("python:annotations", "c");

%pythonbegin %{
import os
import sys

# On Windows (Python 3.8+), register package directory and candidate DLL directories
# so dependent native libraries (netsnmp.dll, libcrypto-*.dll) can be located by LoadLibraryEx.
if sys.platform == "win32" and hasattr(os, "add_dll_directory"):
    _pkg_dir = os.path.dirname(os.path.abspath(__file__))
    if os.path.isdir(_pkg_dir):
        try:
            os.add_dll_directory(_pkg_dir)
        except OSError:
            pass
    for _env_var in ("EZSNMP_NETSNMP_BIN_DIR", "EZSNMP_OPENSSL_BIN_DIR"):
        _cand = os.environ.get(_env_var, "").strip()
        if _cand and os.path.isdir(_cand):
            try:
                os.add_dll_directory(_cand)
            except OSError:
                pass
    for _std_dir in (
        r"C:\ezsnmp-netsnmp-dist\x64\bin\release",
        r"C:\ezsnmp-netsnmp-dist\AMD64\bin\release",
        r"C:\ezsnmp-netsnmp-dist\x86\bin\release",
        r"C:\ezsnmp-netsnmp-dist\ARM64\bin\release",
        r"C:\Program Files\OpenSSL\bin",
        r"C:\Program Files\OpenSSL-Win64\bin",
        r"C:\OpenSSL-Win64\bin",
    ):
        if os.path.isdir(_std_dir):
            try:
                os.add_dll_directory(_std_dir)
            except OSError:
                pass
    for _path_entry in os.environ.get("PATH", "").split(os.pathsep):
        _pe_lower = _path_entry.lower()
        if _path_entry and (
            "netsnmp" in _pe_lower or "snmp" in _pe_lower or "openssl" in _pe_lower
        ):
            if os.path.isdir(_path_entry):
                try:
                    os.add_dll_directory(_path_entry)
                except OSError:
                    pass
%}
#endif

%include <stdint.i>
%include <std_string.i>
%include <std_vector.i>

%include "exceptions.i"
%include "datatypes.i"
%include "config.i"
%include "data_logger.i"
