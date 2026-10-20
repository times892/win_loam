#include "oh_my_loam.h"

#include <fstream>
#include <vector>

#include <pcl/io/pcd_io.h>

#include "common/pcl/pcl_utils.h"
#include "common/registerer/registerer.h"
#include "common/time/timer.h"
#include "oh_my_loam/extractor/extractor_VLP16.h"

namespace oh_my_loam {

namespace {
const double kPointMinDist = 0.5;
}  // namespace

bool OhMyLoam::Init() {
  config_ = common::YAMLConfig::Instance()->config();
  is_vis_ = config_["vis"].as<bool>();
  // Read map save option (default: true)
  if (config_["save_map"]) {
    save_map_ = config_["save_map"].as<bool>();
  }
  // Read trajectory save option (default: true)
  if (config_["save_trajectory"]) {
    save_trajectory_ = config_["save_trajectory"].as<bool>();
  }
  RegisterExtractorVLP16();
  extractor_.reset(common::Registerer<Extractor>::NewInstance(
      "Extractor" + config_["lidar"].as<std::string>()));
  if (!extractor_->Init()) {
    AERROR << "Failed to initialize extractor";
    return false;
  }
  odometer_.reset(new Odometer);
  if (!odometer_->Init()) {
    AERROR << "Failed to initialize odometer";
    return false;
  }
  mapper_.reset(new Mapper);
  if (!mapper_->Init()) {
    AERROR << "Failed to initialize mapper";
    return false;
  }
#ifdef OH_MY_LOAM_ENABLE_VISUALIZER
  if (is_vis_) {
    visualizer_.reset(
        new OhmyloamVisualizer(config_["save_map_path"].as<std::string>()));
  }
#else
  if (is_vis_) {
    AWARN << "Visualizer is disabled in this V1 Win10 no-ROS build.";
    is_vis_ = false;
  }
#endif
  return true;
}

void OhMyLoam::Reset() {
  AWARN << "OhMySlam RESET";
  extractor_->Reset();
  odometer_->Reset();
  mapper_->Reset();
}

void OhMyLoam::Run(double timestamp,
                   const common::PointCloudConstPtr &cloud_in) {
  common::ProfileTimer run_timer("OhMyLoam::Run total");
  common::PointCloudPtr cloud(new common::PointCloud);
  {
    common::ProfileTimer timer("OhMyLoam::RemoveOutliers");
    RemoveOutliers(*cloud_in, cloud.get());
  }
  std::vector<Feature> features;
  {
    common::ProfileTimer timer("OhMyLoam::Extractor::Process");
    extractor_->Process(timestamp, cloud, &features);
  }
  common::Pose3d pose_curr2odom;
  {
    common::ProfileTimer timer("OhMyLoam::Odometer::Process");
    odometer_->Process(timestamp, features, &pose_curr2odom);
  }
  common::Pose3d pose_curr2map;
  const auto &cloud_corn = odometer_->GetCloudCorn()->makeShared();
  const auto &cloud_surf = odometer_->GetCloudSurf()->makeShared();
  {
    common::ProfileTimer timer("OhMyLoam::Mapper::Process dispatch");
    mapper_->Process(timestamp, cloud_corn, cloud_surf, pose_curr2odom,
                     &pose_curr2map);
  }
  // Cache trajectory point (only written on exit)
  if (save_trajectory_) {
    trajectory_.push_back({timestamp, pose_curr2map});
  }
#ifdef OH_MY_LOAM_ENABLE_VISUALIZER
  if (is_vis_) {
    Visualize(pose_curr2map, cloud_corn, cloud_surf, timestamp);
  }
#endif
}

#ifdef OH_MY_LOAM_ENABLE_VISUALIZER
void OhMyLoam::Visualize(const common::Pose3d &pose_curr2map,
                         const TPointCloudConstPtr &cloud_corn,
                         const TPointCloudConstPtr &cloud_surf,
                         double timestamp) {
  std::shared_ptr<OhmyloamVisFrame> frame(new OhmyloamVisFrame);
  frame->timestamp = timestamp;
  frame->cloud_map_corn = mapper_->GetMapCloudCorn();
  frame->cloud_map_surf = mapper_->GetMapCloudSurf();
  frame->cloud_corn = cloud_corn;
  frame->cloud_surf = cloud_surf;
  frame->pose_map = pose_curr2map;
  visualizer_->Render(frame);
}
#endif

void OhMyLoam::RemoveOutliers(const common::PointCloud &cloud_in,
                              common::PointCloud *const cloud_out) const {
  common::RemovePoints<common::Point>(
      cloud_in, cloud_out, [&](const common::Point &pt) {
        return !common::IsFinite(pt) ||
               common::DistanceSquare(pt) < kPointMinDist * kPointMinDist;
      });
}

void OhMyLoam::SaveMap() {
  mapper_->Wait();

  // Determine output directory from save_map_path (used by both map and trajectory)
  std::string save_path = config_["save_map_path"].as<std::string>();
  auto parent_dir = std::filesystem::path(save_path).parent_path();
  if (parent_dir.empty()) {
    parent_dir = std::filesystem::path("./output");
  }
  if (!std::filesystem::exists(parent_dir)) {
    std::filesystem::create_directories(parent_dir);
  }

  // Save map cloud (if enabled)
  if (save_map_) {
    if (save_path.empty()) {
      AWARN << "save_map_path is empty, skip saving map";
    } else {
      TPointCloudPtr map_cloud = mapper_->GetMapCloud();
      if (map_cloud->empty()) {
        AWARN << "Map cloud is empty, skip saving";
      } else {
        common::PointCloud cloud;
        cloud.reserve(map_cloud->size());
        for (const auto &p : *map_cloud) {
          cloud.push_back({p.x, p.y, p.z});
        }
        cloud.width = cloud.size();
        cloud.height = 1;
        cloud.is_dense = false;
        if (pcl::io::savePCDFileASCII(save_path, cloud) == 0) {
          AUSER << "Map saved to " << save_path << " (" << cloud.size()
                << " points)";
        } else {
          AERROR << "Failed to save map to " << save_path;
        }
      }
    }
  } else {
    AINFO << "Map saving is disabled by config, skip";
  }

  // Save trajectory (if enabled, independent of map saving)
  if (!save_trajectory_) {
    AINFO << "Trajectory saving is disabled by config, skip";
    return;
  }
  if (trajectory_.empty()) {
    AWARN << "Trajectory is empty, skip saving";
    return;
  }
  std::string traj_path = (parent_dir / "trajectory.txt").string();
  std::ofstream traj_file(traj_path);
  if (!traj_file.is_open()) {
    AERROR << "Failed to open trajectory file: " << traj_path;
    return;
  }
  traj_file.precision(6);
  for (const auto &pt : trajectory_) {
    const auto &quat = pt.pose.r_quat();
    const auto &trans = pt.pose.t_vec();
    traj_file << pt.timestamp << " " << trans.x() << " " << trans.y() << " "
              << trans.z() << " " << quat.x() << " " << quat.y() << " "
              << quat.z() << " " << quat.w() << "\n";
  }
  traj_file.close();
  AUSER << "Trajectory saved to " << traj_path << " (" << trajectory_.size()
        << " poses)";
}

}  // namespace oh_my_loam
