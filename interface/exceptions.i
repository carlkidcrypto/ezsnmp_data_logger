// SWIG exception translation across supported target languages (python, javascript, csharp, go)
%include <exception.i>

%{
#include "ezsnmp_data_logger/exceptions.h"
%}

%exception {
   try {
      $action
   } catch (const ezsnmp_data_logger::ConfigError& e) {
      SWIG_exception(SWIG_ValueError, e.what());
   } catch (const ezsnmp_data_logger::SnmpError& e) {
      SWIG_exception(SWIG_IOError, e.what());
   } catch (const ezsnmp_data_logger::SinkError& e) {
      SWIG_exception(SWIG_IOError, e.what());
   } catch (const ezsnmp_data_logger::CliError& e) {
      SWIG_exception(SWIG_SyntaxError, e.what());
   } catch (const ezsnmp_data_logger::LoggerError& e) {
      SWIG_exception(SWIG_RuntimeError, e.what());
   } catch (const std::invalid_argument& e) {
      SWIG_exception(SWIG_ValueError, e.what());
   } catch (const std::out_of_range& e) {
      SWIG_exception(SWIG_IndexError, e.what());
   } catch (const std::exception& e) {
      SWIG_exception(SWIG_RuntimeError, e.what());
   }
}
