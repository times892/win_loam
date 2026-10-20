#include "log.h"

#include <filesystem>
#include <sstream>

namespace common {

void InitG3Logging(bool log_to_file, const std::string &prefix,
                   const std::string &path) {
  static std::unique_ptr<g3::LogWorker> worker(
      g3::LogWorker::createLogWorker());
  worker->addSink(std::make_unique<g3::CustomSink>(),
                  &g3::CustomSink::StdLogMessage);
  if (log_to_file) {
    std::filesystem::path log_dir(path.empty() ? "." : path);
    std::filesystem::create_directories(log_dir);
    std::ostringstream file_name;
    if (prefix.empty()) file_name << "g3log";
    file_name << prefix << ".";
    auto now = std::chrono::system_clock::now();
    file_name << g3::localtime_formatted(now, "%Y%m%d-%H%M%S");
    file_name << ".log";
    worker->addSink(std::make_unique<g3::CustomSink>(
                        (log_dir / file_name.str()).string()),
                    &g3::CustomSink::FileLogMessage);
  }
  g3::initializeLogging(worker.get());
}

}  // namespace common
namespace g3 {

CustomSink::~CustomSink() {
  std::ostringstream oss;
  oss << "\ng3log " << (log_to_file_ ? "FileSink" : "StdSink")
      << " shutdown at: ";
  auto now = std::chrono::system_clock::now();
  oss << g3::localtime_formatted(now, "%Y%m%d %H:%M:%S.%f3");
  if (log_to_file_) {
    (*ofs_) << oss.str() << std::endl;
  } else {
    std::clog << oss.str() << std::endl;
  }
};

std::string CustomSink::FormatedMessage(const g3::LogMessage &msg) const {
  std::ostringstream oss;
  oss << "[" << msg.level()[0] << msg.timestamp("%Y%m%d %H:%M:%S.%f3") << " "
      << msg.file() << ":" << msg.line() << "] " << msg.message();
  return oss.str();
}

std::string CustomSink::ColorFormatedMessage(const g3::LogMessage &msg) const {
  std::ostringstream oss;
  oss << GetColorCode(msg._level) << FormatedMessage(msg) << "\033[m";
  return oss.str();
}

std::string CustomSink::GetColorCode(const LEVELS &level) const {
  if (level.value == WARNING.value) {
    return "\033[33m";  // yellow
  }
  if (level.value == DEBUG.value) {
    return "\033[32m";  // green
  }
  if (level.value == G3ERROR.value) {
    return "\033[31m";  // red
  }
  if (level.value == USER.value) {
    return "\033[1m\033[34m";  // bold blue
  }
  if (g3::internal::wasFatal(level)) {
    return "\033[1m\033[31m";  // bold red
  }
  return "\033[97m";  // white
}

}  // namespace g3
