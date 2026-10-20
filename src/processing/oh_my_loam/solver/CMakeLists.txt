aux_source_directory(. SRC_FILES)

add_library(solver STATIC ${SRC_FILES})
target_link_libraries(solver PUBLIC common base oh_my_loam_deps)
