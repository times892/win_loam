cmake_minimum_required(VERSION 3.20)
project(oh_my_loam_win10_noros LANGUAGES CXX)

option(OH_MY_LOAM_BUILD_ROS "Build ROS/catkin example targets" OFF)
option(OH_MY_LOAM_ENABLE_VISUALIZER "Build PCL/VTK visualizer targets" OFF)
option(OH_MY_LOAM_USE_BUNDLED_DEPS "Search dependencies from third_party/win64 first" ON)
option(OH_MY_LOAM_USE_LOCAL_PCL "Use PCL/VTK from third_party/win64/pcl instead of system install" ON)

set(OH_MY_LOAM_THIRD_PARTY_ROOT
    "${PROJECT_SOURCE_DIR}/../../third_party/win64"
    CACHE PATH "Bundled third-party dependency root")

# PCL root: local (third_party) or system install
if(OH_MY_LOAM_USE_LOCAL_PCL)
  set(OH_MY_LOAM_PCL_ROOT_DEFAULT "${OH_MY_LOAM_THIRD_PARTY_ROOT}/pcl")
else()
  set(OH_MY_LOAM_PCL_ROOT_DEFAULT "D:/DevTools/PCL 1.11.1")
endif()
set(OH_MY_LOAM_PCL_ROOT
    "${OH_MY_LOAM_PCL_ROOT_DEFAULT}"
    CACHE PATH "PCL install root (local third_party or system path)")
set(OH_MY_LOAM_PCL_DEBUG_PROPS
    "${CMAKE_CURRENT_LIST_DIR}/../../pcl1.11.1_vs2019_debug.props"
    CACHE FILEPATH "Reference VS2019 Debug x64 PCL property sheet")
set(OH_MY_LOAM_DIST_DIR
    "${PROJECT_SOURCE_DIR}/dist/oh_my_loam"
    CACHE PATH "App-local runtime distribution directory")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

if(MSVC)
  add_compile_options(/permissive- /bigobj /MP)
  # Use Release runtime (MultiThreadedDLL) to match Release-only third-party deps.
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
endif()

if(OH_MY_LOAM_USE_BUNDLED_DEPS)
  # V1 dependency layout:
  #   - Ceres/g3log/yaml-cpp/Eigen live under third_party/win64.
  #   - PCL temporarily comes from OH_MY_LOAM_PCL_ROOT.
  list(PREPEND CMAKE_PREFIX_PATH
       "${OH_MY_LOAM_THIRD_PARTY_ROOT}"
       "${OH_MY_LOAM_THIRD_PARTY_ROOT}/share"
       "${OH_MY_LOAM_PCL_ROOT}"
       "${OH_MY_LOAM_PCL_ROOT}/cmake"
       "${OH_MY_LOAM_PCL_ROOT}/share"
       "${OH_MY_LOAM_PCL_ROOT}/lib/cmake")
endif()

find_package(Eigen3 CONFIG REQUIRED)
find_package(gflags CONFIG REQUIRED)
if(NOT TARGET gflags::gflags)
  add_library(gflags::gflags INTERFACE IMPORTED)
  if(TARGET gflags::gflags_shared)
    target_link_libraries(gflags::gflags INTERFACE gflags::gflags_shared)
  elseif(TARGET gflags_shared)
    target_link_libraries(gflags::gflags INTERFACE gflags_shared)
  else()
    message(FATAL_ERROR "gflags package was found, but no usable gflags target was exported.")
  endif()
endif()
find_package(Ceres CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(g3log CONFIG REQUIRED)

# When visualizer is enabled, include PCL visualization in find_package components
# so that a future PCLConfig discovery won't silently skip it.
if(OH_MY_LOAM_ENABLE_VISUALIZER)
  set(VIS_PCL_COMPONENT visualization)
else()
  set(VIS_PCL_COMPONENT)
endif()

find_package(PCL CONFIG QUIET COMPONENTS common io filters kdtree ${VIS_PCL_COMPONENT})

if(NOT PCL_FOUND)
  if(NOT EXISTS "${OH_MY_LOAM_PCL_ROOT}/include/pcl-1.11/pcl/point_cloud.h")
    message(FATAL_ERROR
      "PCL was not found by config package and OH_MY_LOAM_PCL_ROOT is invalid: "
      "${OH_MY_LOAM_PCL_ROOT}")
  endif()

  message(STATUS "Using manual V1 PCL layout from: ${OH_MY_LOAM_PCL_ROOT}")
  if(EXISTS "${OH_MY_LOAM_PCL_DEBUG_PROPS}")
    message(STATUS "Using PCL Debug props as reference: ${OH_MY_LOAM_PCL_DEBUG_PROPS}")
  endif()

  set(PCL_INCLUDE_DIRS
    "${OH_MY_LOAM_PCL_ROOT}/include/pcl-1.11"
    "${OH_MY_LOAM_PCL_ROOT}/3rdParty/OpenNI2/Include"
    "${OH_MY_LOAM_PCL_ROOT}/3rdParty/Qhull/include"
    "${OH_MY_LOAM_PCL_ROOT}/3rdParty/FLANN/include"
    "${OH_MY_LOAM_PCL_ROOT}/3rdParty/Eigen/eigen3"
    "${OH_MY_LOAM_PCL_ROOT}/3rdParty/Boost/include/boost-1_74")

  link_directories(
    "${OH_MY_LOAM_PCL_ROOT}/lib"
    "${OH_MY_LOAM_PCL_ROOT}/3rdParty/OpenNI2/Lib"
    "${OH_MY_LOAM_PCL_ROOT}/3rdParty/Qhull/lib"
    "${OH_MY_LOAM_PCL_ROOT}/3rdParty/FLANN/lib"
    "${OH_MY_LOAM_PCL_ROOT}/3rdParty/Boost/lib")

  if(OH_MY_LOAM_ENABLE_VISUALIZER)
    link_directories("${OH_MY_LOAM_PCL_ROOT}/3rdParty/VTK/lib")
  endif()

  # Minimal PCL libs for no-visualizer builds.
  # When OH_MY_LOAM_ENABLE_VISUALIZER=ON, pcl_visualization and VTK are
  # appended in the visualizer block below.
  set(PCL_LIBRARIES
    pcl_common.lib
    pcl_io.lib
    pcl_io_ply.lib
    pcl_filters.lib
    pcl_kdtree.lib
    pcl_search.lib
    pcl_octree.lib
    pcl_sample_consensus.lib
    flann_cpp.lib
    flann.lib
    qhullcpp.lib
    qhull_r.lib
    qhull.lib
    OpenNI2.lib
    libboost_filesystem-vc142-mt-x64-1_74.lib
    libboost_iostreams-vc142-mt-x64-1_74.lib
    libboost_system-vc142-mt-x64-1_74.lib
    libboost_thread-vc142-mt-x64-1_74.lib
    libboost_chrono-vc142-mt-x64-1_74.lib
    libboost_date_time-vc142-mt-x64-1_74.lib
    libboost_regex-vc142-mt-x64-1_74.lib)

  set(PCL_DEFINITIONS
    _SCL_SECURE_NO_WARNINGS
    _CRT_SECURE_NO_WARNINGS
    _SILENCE_FPOS_SEEKPOS_DEPRECATION_WARNING
    _HAS_AUTO_PTR_ETC=1
    pop_t=T
    BOOST_USE_WINDOWS_H
    NOMINMAX
    _CRT_SECURE_NO_DEPRECATE)

  set(PCL_RUNTIME_DLLS
    "${OH_MY_LOAM_PCL_ROOT}/bin/pcl_common.dll"
    "${OH_MY_LOAM_PCL_ROOT}/bin/pcl_io.dll"
    "${OH_MY_LOAM_PCL_ROOT}/bin/pcl_io_ply.dll"
    "${OH_MY_LOAM_PCL_ROOT}/bin/pcl_filters.dll"
    "${OH_MY_LOAM_PCL_ROOT}/bin/pcl_kdtree.dll"
    "${OH_MY_LOAM_PCL_ROOT}/bin/pcl_search.dll"
    "${OH_MY_LOAM_PCL_ROOT}/bin/pcl_octree.dll"
    "${OH_MY_LOAM_PCL_ROOT}/bin/pcl_sample_consensus.dll"
    "${OH_MY_LOAM_PCL_ROOT}/bin/OpenNI2.dll"
    "${OH_MY_LOAM_PCL_ROOT}/3rdParty/FLANN/bin/flann_cpp.dll"
    "${OH_MY_LOAM_PCL_ROOT}/3rdParty/FLANN/bin/flann.dll"
    "${OH_MY_LOAM_PCL_ROOT}/3rdParty/Qhull/bin/qhull.dll"
    "${OH_MY_LOAM_PCL_ROOT}/3rdParty/Qhull/bin/qhull_r.dll"
    "${OH_MY_LOAM_PCL_ROOT}/3rdParty/Qhull/bin/qhull_p.dll")

  # --- Visualizer additions (PCL visualization + VTK) ---
  if(OH_MY_LOAM_ENABLE_VISUALIZER)
    list(APPEND PCL_INCLUDE_DIRS
      "${OH_MY_LOAM_PCL_ROOT}/3rdParty/VTK/include/vtk-8.2")

    list(APPEND PCL_LIBRARIES
      pcl_visualization.lib)

    # VTK Release libs: full list from pcl1.11.1_vs2019_release layout
    list(APPEND PCL_LIBRARIES
      vtkChartsCore-8.2.lib
      vtkCommonColor-8.2.lib
      vtkCommonComputationalGeometry-8.2.lib
      vtkCommonCore-8.2.lib
      vtkCommonDataModel-8.2.lib
      vtkCommonExecutionModel-8.2.lib
      vtkCommonMath-8.2.lib
      vtkCommonMisc-8.2.lib
      vtkCommonSystem-8.2.lib
      vtkCommonTransforms-8.2.lib
      vtkDICOMParser-8.2.lib
      vtkDomainsChemistry-8.2.lib
      vtkDomainsChemistryOpenGL2-8.2.lib
      vtkdoubleconversion-8.2.lib
      vtkexodusII-8.2.lib
      vtkexpat-8.2.lib
      vtkFiltersAMR-8.2.lib
      vtkFiltersCore-8.2.lib
      vtkFiltersExtraction-8.2.lib
      vtkFiltersFlowPaths-8.2.lib
      vtkFiltersGeneral-8.2.lib
      vtkFiltersGeneric-8.2.lib
      vtkFiltersGeometry-8.2.lib
      vtkFiltersHybrid-8.2.lib
      vtkFiltersHyperTree-8.2.lib
      vtkFiltersImaging-8.2.lib
      vtkFiltersModeling-8.2.lib
      vtkFiltersParallel-8.2.lib
      vtkFiltersParallelImaging-8.2.lib
      vtkFiltersPoints-8.2.lib
      vtkFiltersProgrammable-8.2.lib
      vtkFiltersSelection-8.2.lib
      vtkFiltersSMP-8.2.lib
      vtkFiltersSources-8.2.lib
      vtkFiltersStatistics-8.2.lib
      vtkFiltersTexture-8.2.lib
      vtkFiltersTopology-8.2.lib
      vtkFiltersVerdict-8.2.lib
      vtkfreetype-8.2.lib
      vtkGeovisCore-8.2.lib
      vtkgl2ps-8.2.lib
      vtkglew-8.2.lib
      vtkGUISupportMFC-8.2.lib
      vtkhdf5-8.2.lib
      vtkhdf5_hl-8.2.lib
      vtkImagingColor-8.2.lib
      vtkImagingCore-8.2.lib
      vtkImagingFourier-8.2.lib
      vtkImagingGeneral-8.2.lib
      vtkImagingHybrid-8.2.lib
      vtkImagingMath-8.2.lib
      vtkImagingMorphological-8.2.lib
      vtkImagingSources-8.2.lib
      vtkImagingStatistics-8.2.lib
      vtkImagingStencil-8.2.lib
      vtkInfovisCore-8.2.lib
      vtkInfovisLayout-8.2.lib
      vtkInteractionImage-8.2.lib
      vtkInteractionStyle-8.2.lib
      vtkInteractionWidgets-8.2.lib
      vtkIOAMR-8.2.lib
      vtkIOAsynchronous-8.2.lib
      vtkIOCityGML-8.2.lib
      vtkIOCore-8.2.lib
      vtkIOEnSight-8.2.lib
      vtkIOExodus-8.2.lib
      vtkIOExport-8.2.lib
      vtkIOExportOpenGL2-8.2.lib
      vtkIOExportPDF-8.2.lib
      vtkIOGeometry-8.2.lib
      vtkIOImage-8.2.lib
      vtkIOImport-8.2.lib
      vtkIOInfovis-8.2.lib
      vtkIOLSDyna-8.2.lib
      vtkIOLegacy-8.2.lib
      vtkIOMINC-8.2.lib
      vtkIOMovie-8.2.lib
      vtkIONetCDF-8.2.lib
      vtkIOPLY-8.2.lib
      vtkIOParallel-8.2.lib
      vtkIOParallelXML-8.2.lib
      vtkIOSQL-8.2.lib
      vtkIOSegY-8.2.lib
      vtkIOTecplotTable-8.2.lib
      vtkIOVeraOut-8.2.lib
      vtkIOVideo-8.2.lib
      vtkIOXML-8.2.lib
      vtkIOXMLParser-8.2.lib
      vtkjsoncpp-8.2.lib
      vtklibharu-8.2.lib
      vtklibxml2-8.2.lib
      vtklz4-8.2.lib
      vtklzma-8.2.lib
      vtkmetaio-8.2.lib
      vtknetcdf-8.2.lib
      vtkogg-8.2.lib
      vtkParallelCore-8.2.lib
      vtkpng-8.2.lib
      vtkproj-8.2.lib
      vtkpugixml-8.2.lib
      vtkRenderingAnnotation-8.2.lib
      vtkRenderingContext2D-8.2.lib
      vtkRenderingContextOpenGL2-8.2.lib
      vtkRenderingCore-8.2.lib
      vtkRenderingExternal-8.2.lib
      vtkRenderingFreeType-8.2.lib
      vtkRenderingGL2PSOpenGL2-8.2.lib
      vtkRenderingImage-8.2.lib
      vtkRenderingLOD-8.2.lib
      vtkRenderingLabel-8.2.lib
      vtkRenderingOpenGL2-8.2.lib
      vtkRenderingVolume-8.2.lib
      vtkRenderingVolumeOpenGL2-8.2.lib
      vtksqlite-8.2.lib
      vtksys-8.2.lib
      vtktiff-8.2.lib
      vtkverdict-8.2.lib
      vtkViewsContext2D-8.2.lib
      vtkViewsCore-8.2.lib
      vtkViewsInfovis-8.2.lib
      vtkzlib-8.2.lib)

    # VTK runtime DLLs for Release
    file(GLOB VTK_RELEASE_DLLS
         "${OH_MY_LOAM_PCL_ROOT}/3rdParty/VTK/bin/vtk*-8.2.dll")

    list(APPEND PCL_RUNTIME_DLLS
      "${OH_MY_LOAM_PCL_ROOT}/bin/pcl_visualization.dll"
      ${VTK_RELEASE_DLLS})

    message(STATUS "Visualizer enabled: added pcl_visualization + VTK 8.2 Release")
  endif()
endif()

set(OH_MY_LOAM_RUNTIME_DLLS
  "${OH_MY_LOAM_THIRD_PARTY_ROOT}/bin/ceres.dll"
  "${OH_MY_LOAM_THIRD_PARTY_ROOT}/bin/glog.dll"
  "${OH_MY_LOAM_THIRD_PARTY_ROOT}/bin/gflags.dll"
  "${OH_MY_LOAM_THIRD_PARTY_ROOT}/bin/g3log.dll"
  ${PCL_RUNTIME_DLLS})

add_library(oh_my_loam_deps INTERFACE)
target_include_directories(oh_my_loam_deps INTERFACE
  "${PROJECT_SOURCE_DIR}"
  "${OH_MY_LOAM_THIRD_PARTY_ROOT}/include"
  ${PCL_INCLUDE_DIRS}
  ${CERES_INCLUDE_DIRS}
  ${YAML_CPP_INCLUDE_DIRS}
  ${G3LOG_INCLUDE_DIRS}
)

target_link_libraries(oh_my_loam_deps INTERFACE
  ${PCL_LIBRARIES}
)

target_compile_definitions(oh_my_loam_deps INTERFACE ${PCL_DEFINITIONS})

if(TARGET Eigen3::Eigen)
  target_link_libraries(oh_my_loam_deps INTERFACE Eigen3::Eigen)
endif()
if(TARGET Ceres::ceres)
  target_link_libraries(oh_my_loam_deps INTERFACE Ceres::ceres)
endif()
if(TARGET yaml-cpp::yaml-cpp)
  target_link_libraries(oh_my_loam_deps INTERFACE yaml-cpp::yaml-cpp)
elseif(TARGET yaml-cpp)
  target_link_libraries(oh_my_loam_deps INTERFACE yaml-cpp)
endif()
if(TARGET g3log)
  target_link_libraries(oh_my_loam_deps INTERFACE g3log)
elseif(TARGET g3log::g3log)
  target_link_libraries(oh_my_loam_deps INTERFACE g3log::g3log)
endif()

if(OH_MY_LOAM_ENABLE_VISUALIZER)
  target_compile_definitions(oh_my_loam_deps INTERFACE OH_MY_LOAM_ENABLE_VISUALIZER)
endif()

add_subdirectory(common)
add_subdirectory(oh_my_loam)
add_subdirectory(examples)
