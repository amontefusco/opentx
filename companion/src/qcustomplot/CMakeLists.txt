set(qcustomplot_SRCS
  qcustomplot.cpp
)

set(qcustomplot_HDRS
  qcustomplot.h
)

IF ( QT_VERSION EQUAL "4" )
qt4_wrap_cpp(qcustomplot_SRCS ${qcustomplot_HDRS})
ELSEIF ( QT_VERSION EQUAL "5" )
qt5_wrap_cpp(qcustomplot_SRCS ${qcustomplot_HDRS})
ENDIF()

add_library(qcustomplot ${qcustomplot_SRCS})
