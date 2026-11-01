%module ezsnmp_data_logger

#ifdef SWIGPYTHON
%feature("autodoc", "2");
%feature("python:annotations", "c");
#endif

%include <stdint.i>
#ifdef SWIGXML
namespace std {
   class string {};
   template <class T>
   class vector {};
}
#else
%include <std_string.i>
%include <std_vector.i>
#endif

%include "exceptions.i"
%include "datatypes.i"
%include "config.i"
%include "data_logger.i"
