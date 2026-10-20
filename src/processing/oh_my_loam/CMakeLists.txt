add_subdirectory(base)
add_subdirectory(extractor)
add_subdirectory(odometer)
add_subdirectory(mapper)
add_subdirectory(solver)

if(OH_MY_LOAM_ENABLE_VISUALIZER)
  add_subdirectory(visualizer)
endif()

aux_source_directory(. SRC_FILES)

add_library(oh_my_loam STATIC ${SRC_FILES})
target_link_libraries(oh_my_loam PUBLIC
  common
  base
  extractor
  odometer
  mapper
  solver
  oh_my_loam_deps
)

if(OH_MY_LOAM_ENABLE_VISUALIZER)
  target_link_libraries(oh_my_loam PUBLIC visualizer)
endif()
