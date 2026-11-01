%{
#include "ezsnmp_data_logger/config.h"
%}

%template(StringVector) std::vector<std::string>;

%include "../include/ezsnmp_data_logger/config.h"

%template(MetricConfigVector) std::vector<ezsnmp_data_logger::MetricConfig>;
%template(DutConfigVector) std::vector<ezsnmp_data_logger::DutConfig>;
%template(SinkConfigVector) std::vector<ezsnmp_data_logger::SinkConfig>;

#ifdef SWIGPYTHON
%extend ezsnmp_data_logger::LoggerConfig {
   std::string __repr__() const {
      return "LoggerConfig(duts=" + std::to_string($self->duts.size()) +
             ", sinks=" + std::to_string($self->sinks.size()) + ")";
   }
}
#endif
