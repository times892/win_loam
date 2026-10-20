aux_source_directory(. SRC_FILES)

add_library(mapper STATIC ${SRC_FILES})
target_link_libraries(mapper PUBLIC common base solver oh_my_loam_deps)
