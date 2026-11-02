%{
#include "ezsnmp_data_logger/datatypes.h"
%}

%include "../include/ezsnmp_data_logger/datatypes.h"

%template(SampleVector) std::vector<ezsnmp_data_logger::Sample>;
%template(BatchVector) std::vector<ezsnmp_data_logger::Batch>;
%template(DutProbeResultVector) std::vector<ezsnmp_data_logger::DutProbeResult>;
%template(InspectRowVector) std::vector<ezsnmp_data_logger::InspectRow>;

#ifdef SWIGPYTHON
%extend ezsnmp_data_logger::Sample {
   std::string __repr__() const {
      return "Sample(metric_name='" + $self->metric_name + "', oid='" + $self->oid +
             "', value='" + $self->display_value() + "', status=" +
             std::to_string($self->status) + ")";
   }
}

%extend ezsnmp_data_logger::Batch {
   std::string __repr__() const {
      return "Batch(dut_name='" + $self->dut_name + "', dut_ip='" + $self->dut_ip +
             "', samples=" + std::to_string($self->samples.size()) + ")";
   }
}

%extend ezsnmp_data_logger::LoggerStats {
   std::string __repr__() const {
      return $self->to_json(-1);
   }
}
#endif
