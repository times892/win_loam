aux_source_directory(. SRC_FILES)

add_library(visualizer STATIC ${SRC_FILES})
target_link_libraries(visualizer PUBLIC common base solver oh_my_loam_deps)
