%{
#include "ezsnmp_data_logger/version.h"
#include "ezsnmp_data_logger/data_logger.h"
#include "ezsnmp_data_logger/cli_runner.h"
%}

#ifdef SWIGPYTHON
%thread ezsnmp_data_logger::DataLogger::run;
%thread ezsnmp_data_logger::DataLogger::run_once;
%thread ezsnmp_data_logger::DataLogger::stop;
%thread ezsnmp_data_logger::DataLogger::probe;
%thread ezsnmp_data_logger::DataLogger::probe_json;
%thread ezsnmp_data_logger::DataLogger::request_json;
%thread ezsnmp_data_logger::DataLogger::set_json;
%thread ezsnmp_data_logger::CliRunner::run;
%thread ezsnmp_data_logger::CliRunner::execute;
#endif

%include "../include/ezsnmp_data_logger/version.h"
%include "../include/ezsnmp_data_logger/data_logger.h"
%include "../include/ezsnmp_data_logger/cli_runner.h"
