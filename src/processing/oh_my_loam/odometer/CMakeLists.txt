aux_source_directory(. SRC_FILES)

add_library(odometer STATIC ${SRC_FILES})
target_link_libraries(odometer PUBLIC common base solver oh_my_loam_deps)
