aux_source_directory(. SRC_FILES)

add_library(base STATIC ${SRC_FILES})
target_link_libraries(base PUBLIC common oh_my_loam_deps)
