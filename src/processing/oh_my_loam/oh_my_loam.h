#pragma once

#include <yaml-cpp/node/node.h>

#include <filesystem>
#include <vector>

#include "common/common.h"
#include "oh_my_loam/extractor/extractor.h"
#include "oh_my_loam/mapper/mapper.h"
#include "oh_my_loam/odometer/odometer.h"

#ifdef OH_MY_LOAM_ENABLE_VISUALIZER
#include "oh_my_loam/visualizer/ohmyloam_visualizer.h"
#endif

namespace oh_my_loam {

// Trajectory point for saving
struct TrajectoryPoint {
  double timestamp;
  common::Pose3d pose;
};

class OhMyLoam {
 public:
  OhMyLoam() = default;

  bool Init();

  void Run(double timestamp, const common::PointCloudConstPtr &cloud_in);

  void SaveMap();

 private:
  void Reset();

#ifdef OH_MY_LOAM_ENABLE_VISUALIZER
  void Visualize(const common::Pose3d &pose_curr2map,
                 const TPointCloudConstPtr &cloud_corn,
                 const TPointCloudConstPtr &cloud_surf, double timestamp = 0.0);
#endif

  // remove outliers: nan or very close points
  void RemoveOutliers(const common::PointCloud &cloud_in,
                      common::PointCloud *const cloud_out) const;

  std::unique_ptr<Extractor> extractor_{nullptr};

  std::unique_ptr<Odometer> odometer_{nullptr};

  std::unique_ptr<Mapper> mapper_{nullptr};

  YAML::Node config_;
  bool is_vis_ = false;
  bool save_map_ = true;      // enable map output
  bool save_trajectory_ = true;  // enable trajectory output

  // Trajectory cache: only written to file on exit
  std::vector<TrajectoryPoint> trajectory_;

#ifdef OH_MY_LOAM_ENABLE_VISUALIZER
  std::unique_ptr<OhmyloamVisualizer> visualizer_{nullptr};
#endif

  DISALLOW_COPY_AND_ASSIGN(OhMyLoam);
};

}  // namespace oh_my_loam
