include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(shared_SRCS
  genericpanel.cpp
  hexspinbox.cpp
  verticalscrollarea.cpp
)

set(shared_HDRS
  autocheckbox.h
  autocombobox.h
  autodoublespinbox.h
  autohexspinbox.h
  autolineedit.h
  genericpanel.h
  hexspinbox.h
  verticalscrollarea.h
)

IF ( QT_VERSION EQUAL "4" )
qt4_wrap_cpp(shared_SRCS ${shared_HDRS})
ELSEIF ( QT_VERSION EQUAL "5" )
qt5_wrap_cpp(shared_SRCS ${shared_HDRS})
ENDIF()

add_library(shared ${shared_SRCS})