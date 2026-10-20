add_executable(oh_my_loam_noros main_noros.cc)
target_link_libraries(oh_my_loam_noros PRIVATE
  oh_my_loam
  extractor
  odometer
  mapper
  solver
  base
  common
  oh_my_loam_deps
)

if(OH_MY_LOAM_ENABLE_VISUALIZER)
  target_link_libraries(oh_my_loam_noros PRIVATE visualizer)
endif()

add_custom_command(TARGET oh_my_loam_noros POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:oh_my_loam_noros>"
)

foreach(runtime_dll IN LISTS OH_MY_LOAM_RUNTIME_DLLS)
  if(EXISTS "${runtime_dll}")
    add_custom_command(TARGET oh_my_loam_noros POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${runtime_dll}"
              "$<TARGET_FILE_DIR:oh_my_loam_noros>")
  else()
    message(WARNING "Runtime DLL not found and will not be copied: ${runtime_dll}")
  endif()
endforeach()

if(OH_MY_LOAM_BUILD_ROS)
  # ROS build placeholder:
  # V1 does not build ROS/catkin targets. Restore this section only after the
  # Win10 no-ROS executable is stable.
  find_package(catkin REQUIRED COMPONENTS
    geometry_msgs
    nav_msgs
    sensor_msgs
    roscpp
    rospy
    rosbag
    std_msgs
  )

  add_executable(main_rosbag main_rosbag.cc)
  target_link_libraries(main_rosbag PRIVATE
    ${catkin_LIBRARIES}
    oh_my_loam
    extractor
    odometer
    mapper
    solver
    base
    common
    oh_my_loam_deps
  )
endif()
