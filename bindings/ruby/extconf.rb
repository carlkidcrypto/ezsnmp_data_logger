require "mkmf"

$CXXFLAGS << " -std=c++17 -I#{File.expand_path('../../include', __dir__)} "
have_library("ezsnmp_data_logger")
create_makefile("ezsnmp_data_logger")
