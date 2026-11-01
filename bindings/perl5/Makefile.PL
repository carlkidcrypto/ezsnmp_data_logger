use ExtUtils::MakeMaker;

WriteMakefile(
    NAME      => 'ezsnmp_data_logger',
    VERSION   => '0.2.0',
    CCFLAGS   => '-std=c++17',
    INC       => '-I../../include',
    LIBS      => ['-L../../build -lezsnmp_data_logger'],
    OBJECT    => 'ezsnmp_data_logger_wrap.o',
);
