project(data_broker_graph_view)
set(PROJECT_VERSION 1.0)
set(PROJECT_DESCRIPTION "Graph editor to create connections in data broker.")
cmake_minimum_required(VERSION 2.6)
include(FindPkgConfig)

find_package(lib_manager)
lib_defaults()
define_module_info()

set (QT_USE_QTOPENGL TRUE)
setup_qt()

if(USE_QT5)
    pkg_check_modules(OSGQT openscenegraph-osgQt5)
else(USE_QT5)
    pkg_check_modules(OSGQT openscenegraph-osgQt)
endif(USE_QT5)

if(OSGQT_FOUND)
  include_directories(${OSGQT_INCLUDE_DIRS})
  link_directories(${OSGQT_LIBRARY_DIRS})
  find_package(OpenSceneGraph REQUIRED osgManipulator osgViewer osgFX osgShadow osgParticle osgTerrain osgDB osgGA osgWidget osgText osgUtil)
else()
  find_package(OpenSceneGraph REQUIRED osgManipulator osgViewer osgFX osgShadow osgParticle osgTerrain osgDB osgGA osgWidget osgText osgUtil osgQt)
endif()

include_directories(${OPENSCENEGRAPH_INCLUDE_DIRS})
link_directories(${OPENSCENEGRAPH_LIBRARY_DIRS})



pkg_check_modules(PKGCONFIG REQUIRED
        lib_manager
        cfg_manager
        data_broker
        configmaps
        main_gui
        mars_utils
        osg_text
        osg_graph_viz
)
include_directories(${PKGCONFIG_INCLUDE_DIRS})
link_directories(${PKGCONFIG_LIBRARY_DIRS})
add_definitions(${PKGCONFIG_CLFAGS_OTHER})  #flags excluding the ones with -I

include_directories(src)

set(SOURCES 
	src/DataBrokerGraphView.cpp
	src/GraphGui.cpp
)

set(HEADERS
	src/DataBrokerGraphView.hpp
	src/GraphGui.hpp
)

set (QT_MOC_HEADER
	src/GraphGui.hpp
)

if (${USE_QT5})
qt5_wrap_ui( SOURCES_UI_H ${SOURCES_UI})
qt5_wrap_cpp ( QT_MOC_HEADER_SRC ${QT_MOC_HEADER} )
else (${USE_QT5})
qt4_wrap_ui( SOURCES_UI_H ${SOURCES_UI})
qt4_wrap_cpp ( QT_MOC_HEADER_SRC ${QT_MOC_HEADER} )
endif (${USE_QT5})

cmake_minimum_required(VERSION 2.6)

add_library(${PROJECT_NAME} SHARED ${SOURCES} ${QT_MOC_HEADER_SRC} ${SOURCES_UI_H} )

if (${USE_QT5})
qt5_use_modules(${PROJECT_NAME} Widgets OpenGL)
endif (${USE_QT5})

target_link_libraries(${PROJECT_NAME}
                      ${PKGCONFIG_LIBRARIES}
                      ${QT_LIBRARIES}
                      ${OPENSCENEGRAPH_LIBRARIES}
                      ${OSGQT_LIBRARIES}
)

if(WIN32)
  set(LIB_INSTALL_DIR bin) # .dll are in PATH, like executables
else(WIN32)
  set(LIB_INSTALL_DIR lib)
endif(WIN32)


set(_INSTALL_DESTINATIONS
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION ${LIB_INSTALL_DIR}
	ARCHIVE DESTINATION lib
)


# Install the library into the lib folder
install(TARGETS ${PROJECT_NAME} ${_INSTALL_DESTINATIONS})

# Install headers into mars include directory
install(FILES ${HEADERS} DESTINATION include/${PROJECT_NAME})

#INSTALL(DIRECTORY resources DESTINATION share/data_broker_widget)

# Prepare and install necessary files to support finding of the library 
# using pkg-config
configure_file(${PROJECT_NAME}.pc.in ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.pc DESTINATION lib/pkgconfig)


