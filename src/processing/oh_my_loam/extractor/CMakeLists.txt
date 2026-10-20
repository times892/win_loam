aux_source_directory(. SRC_FILES)

add_library(extractor STATIC ${SRC_FILES})
target_link_libraries(extractor PUBLIC common base oh_my_loam_deps)
