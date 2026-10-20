file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS *.cc)

if(NOT OH_MY_LOAM_ENABLE_VISUALIZER)
  list(FILTER SRC_FILES EXCLUDE REGEX "/visualizer/")
endif()

add_library(common STATIC ${SRC_FILES})
target_link_libraries(common PUBLIC oh_my_loam_deps)
