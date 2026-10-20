#include <pcl/io/pcd_io.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <csignal>
#include <filesystem>
#include <functional>
#include <regex>
#include <set>
#include <string>
#include <thread>

#include "common/common.h"
#include "oh_my_loam/oh_my_loam.h"

using namespace common;
using namespace oh_my_loam;
namespace fs = std::filesystem;

void PointCloudHandler(const PointCloudConstPtr &cloud, double timestamp,
                       OhMyLoam *const slam);

// Parse timestamp from filename:
// Format 1: frame_{seq:06d}_{timestamp:.6f}.pcd (with embedded timestamp)
// Format 2: (Frame XXXX).pcd (seq only, no timestamp)
bool ParseTimestampFromFilename(const fs::path &path, double &timestamp,
                                uint32_t &seq) {
  // Try format 1: frame_000001_1783244466.974544.pcd
  static std::regex frame_regex(
      R"(frame_(\d{6})_([\d.]+)\.pcd$)",
      std::regex_constants::icase);
  std::smatch match;
  std::string filename = path.filename().string();
  if (std::regex_search(filename, match, frame_regex) && match.size() == 3) {
    seq = std::stoul(match[1].str());
    timestamp = std::stod(match[2].str());
    return true;
  }
  // Try format 2: (Frame 0450).pcd
  static std::regex frame_paren_regex(
      R"(\(Frame\s+(\d+)\)\.pcd$)",
      std::regex_constants::icase);
  if (std::regex_search(filename, match, frame_paren_regex) && match.size() == 2) {
    seq = std::stoul(match[1].str());
    timestamp = 0.0;  // No timestamp in filename, caller should use fallback
    return false;     // Return false to trigger fallback to system clock
  }
  return false;
}

// Global flag for graceful exit
std::atomic<bool> to_exit_process{false};

void SignalHandler(int signum) {
  AUSER << "Received signal " << signum << ", exiting gracefully...";
  to_exit_process = true;
}

int main(int argc, char *argv[]) {
  // Register signal handler for graceful exit
  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);

  if (argc < 3) {
    std::cerr << "\033[1m\033[31mUsage: " << argv[0]
              << " <config.yaml> <pcd_dir> [--watch]\033[m" << std::endl;
    std::cerr << "  <pcd_dir>       : Directory containing PCD files"
              << std::endl;
    std::cerr << "  --watch         : Enable incremental monitoring mode"
              << std::endl;
    return -1;
  }

  std::string config_file = argv[1];
  std::string pcd_dir = argv[2];
  bool watch_mode = (argc >= 4 && std::string(argv[3]) == "--watch");

  // config
  YAMLConfig::Instance()->Init(config_file);
  bool is_log_to_file = YAMLConfig::Instance()->Get<bool>("log_to_file");
  std::string log_path = YAMLConfig::Instance()->Get<std::string>("log_path");
  std::string lidar = YAMLConfig::Instance()->Get<std::string>("lidar");
  // logging
  InitG3Logging(is_log_to_file, "oh_my_loam_" + lidar, log_path);
  const auto &config = YAMLConfig::Instance()->config();
  bool profile_timing =
      config["profile_timing"] ? config["profile_timing"].as<bool>() : false;
  Profiler::SetEnabled(profile_timing);
  AINFO << "Profile timing: " << (profile_timing ? "ON" : "OFF");
  AUSER << "LOAM start..., lidar = " << lidar;
  AUSER << "Mode: " << (watch_mode ? "WATCH (incremental)" : "OFFLINE (batch)");

  // SLAM system
  OhMyLoam slam;
  if (!slam.Init()) {
    AFATAL << "Failed to initialize slam system.";
  }

  // Validate directory
  ACHECK(fs::exists(pcd_dir)) << "Directory not exist: " << pcd_dir;

  if (watch_mode) {
    // === WATCH MODE: Incremental monitoring ===
    AUSER << "Watching directory: " << pcd_dir;
    std::set<fs::path> processed_files;
    size_t frame_id = 0;

    while (!to_exit_process) {
      // Scan directory for new PCD files
      std::vector<fs::path> new_files;
      for (auto &it : fs::directory_iterator(pcd_dir)) {
        if (!fs::is_regular_file(it.path())) continue;
        std::string ext = it.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (ext == ".pcd" && processed_files.find(it.path()) == processed_files.end()) {
          new_files.push_back(it.path());
        }
      }

      // Sort by filename (seq order)
      std::sort(new_files.begin(), new_files.end());

      // Process new files
      for (auto &path : new_files) {
        if (to_exit_process) break;

        PointCloudPtr cloud(new PointCloud);
        {
          ProfileTimer timer("main_noros::loadPCDFile");
          if (pcl::io::loadPCDFile(path.string(), *cloud) < 0) {
            AWARN << "Failed to load PCD file: " << path.string()
                  << ", skipping...";
            processed_files.insert(path);
            continue;
          }
        }

        // Parse timestamp from filename
        double timestamp = 0.0;
        uint32_t seq = 0;
        if (!ParseTimestampFromFilename(path, timestamp, seq)) {
          // Fallback to system clock if parsing fails
          auto millisecs = std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now().time_since_epoch());
          timestamp = millisecs.count() / 1000.0;
          AWARN << "Failed to parse timestamp from: " << path.filename()
                << ", using system clock";
        }

        PointCloudHandler(cloud, timestamp, &slam);
        processed_files.insert(path);
        frame_id++;
      }

      // Sleep briefly if no new files
      if (new_files.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
    }

    AUSER << "Watch mode stopped. Processed " << frame_id << " frames.";
  } else {
    // === OFFLINE MODE: Batch processing (original logic) ===
    std::vector<fs::path> cloud_paths;
    for (auto &it : fs::directory_iterator(pcd_dir)) {
      if (!fs::is_regular_file(it.path())) continue;
      std::string ext = it.path().extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      if (ext == ".pcd") cloud_paths.push_back(it.path());
    }
    AWARN_IF(cloud_paths.empty())
        << "No point cloud file in directory: " << pcd_dir;
    AINFO_IF(!cloud_paths.empty())
        << "There are " << cloud_paths.size() << " point clouds in total";
    std::sort(cloud_paths.begin(), cloud_paths.end());

    // Load point cloud and process
    for (auto &path : cloud_paths) {
      if (to_exit_process) {
        AUSER << "Interrupted, stopping early...";
        break;
      }

      PointCloudPtr cloud(new PointCloud);
      {
        ProfileTimer timer("main_noros::loadPCDFile");
        if (pcl::io::loadPCDFile(path.string(), *cloud) < 0) {
          AWARN << "Failed to load PCD file: " << path.string()
                << ", skipping...";
          continue;
        }
      }

      // Parse timestamp from filename
      double timestamp = 0.0;
      uint32_t seq = 0;
      if (!ParseTimestampFromFilename(path, timestamp, seq)) {
        // Fallback to system clock if parsing fails
        auto millisecs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch());
        timestamp = millisecs.count() / 1000.0;
        AWARN << "Failed to parse timestamp from: " << path.filename()
              << ", using system clock";
      }

      PointCloudHandler(cloud, timestamp, &slam);
    }
  }

  // Save map before exit
  AUSER << "Saving map...";
  slam.SaveMap();
  AUSER << "LOAM finished.";
  return 0;
}

void PointCloudHandler(const PointCloudConstPtr &cloud, double timestamp,
                       OhMyLoam *const slam) {
  static size_t frame_id = 0;
  AINFO << "Ohmyloam: frame_id = " << ++frame_id
        << ", timestamp = " << FMT_TIMESTAMP(timestamp)
        << ", point_number = " << cloud->size();
  ProfileTimer timer("PointCloudHandler::Run frame_" + std::to_string(frame_id));
  slam->Run(timestamp, cloud);
}
