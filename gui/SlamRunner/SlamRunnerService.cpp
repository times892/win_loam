#pragma execution_character_set("utf-8")
#include "SlamRunnerService.h"

using namespace System::Text;

namespace SlamRunner {

    SlamRunnerService::SlamRunnerService() {
        _hasRun = false;
        _state = SlamRunState::Idle;
        _request = gcnew SlamRunRequest();
        _process = nullptr;
        _runThread = nullptr;
        _logWriter = nullptr;
        _runStopwatch = nullptr;
        _stopRequested = false;
        _pcdTotalCount = 0;
        _processedCount = 0;
        _uiFrameLogInterval = 50;
        _lastUiLogFrame = 0;
        _runId = nullptr;
        _resolvedCoreExePath = nullptr;
        _resolvedCoreDirectory = nullptr;
        _runtimeConfigPath = nullptr;
        _runOutputDirectory = nullptr;
        _runLogDirectory = nullptr;
        _logFilePath = nullptr;
        _mapOutputPath = nullptr;
        _trajectoryOutputPath = nullptr;
    }

    SlamRunnerService::~SlamRunnerService() {
        Cleanup();
    }

    SlamRunState SlamRunnerService::State::get() {
        return _state;
    }

    String^ SlamRunnerService::OutputDirectory::get() {
        return _request->OutputDirectory;
    }

    void SlamRunnerService::OutputDirectory::set(String^ value) {
        _request->OutputDirectory = value;
    }

    String^ SlamRunnerService::LastRunId::get() {
        return _runId;
    }

    String^ SlamRunnerService::LastRunOutputDirectory::get() {
        return _runOutputDirectory;
    }

    String^ SlamRunnerService::LastLogFilePath::get() {
        return _logFilePath;
    }

    void SlamRunnerService::LoadPointCloud(String^ pcdDirectory) {
        if (String::IsNullOrWhiteSpace(pcdDirectory)) {
            throw gcnew ArgumentException("PCD directory is empty.");
        }

        String^ fullPath = Path::GetFullPath(pcdDirectory);
        int pcdCount = ValidatePcdDirectory(fullPath);

        _request->PcdDirectory = fullPath;
        _pcdTotalCount = pcdCount;
    }

    void SlamRunnerService::LoadConfig(String^ yamlPath) {
        if (String::IsNullOrWhiteSpace(yamlPath)) {
            throw gcnew ArgumentException("Config file path is empty.");
        }

        String^ fullPath = Path::GetFullPath(yamlPath);
        if (!File::Exists(fullPath)) {
            throw gcnew FileNotFoundException(
                String::Format("Config file not found: {0}", fullPath));
        }

        String^ ext = Path::GetExtension(fullPath);
        if (!String::Equals(ext, ".yaml", StringComparison::OrdinalIgnoreCase) &&
            !String::Equals(ext, ".yml", StringComparison::OrdinalIgnoreCase)) {
            throw gcnew ArgumentException("Config file must be yaml/yml format.");
        }

        _request->ConfigPath = fullPath;
    }

    void SlamRunnerService::Run() {
        if (_hasRun) {
            throw gcnew InvalidOperationException("SlamRunnerService can only run once.");
        }

        if (String::IsNullOrWhiteSpace(_request->PcdDirectory)) {
            throw gcnew InvalidOperationException("PCD directory not set.");
        }
        if (String::IsNullOrWhiteSpace(_request->ConfigPath)) {
            throw gcnew InvalidOperationException("Config file not set.");
        }

        _hasRun = true;
        _stopRequested = false;
        _processedCount = 0;
        _lastUiLogFrame = 0;
        _runStopwatch = nullptr;

        SetState(SlamRunState::Loading);

        _runThread = gcnew Thread(gcnew ThreadStart(this, &SlamRunnerService::RunWorker));
        _runThread->IsBackground = true;
        _runThread->Name = "SlamRunnerWorker";
        _runThread->Start();
    }

    void SlamRunnerService::Stop() {
        if (_state != SlamRunState::Running && _state != SlamRunState::Loading) {
            return;
        }

        _stopRequested = true;
        SetState(SlamRunState::Stopping);
        EmitLog("Stop requested by GUI.");

        try {
            if (_process != nullptr && !_process->HasExited) {
                _process->Kill();
                EmitLog("Core process was killed.");
            }
        }
        catch (Exception^ ex) {
            EmitLog(String::Format("Warning while stopping process: {0}", ex->Message));
        }
    }

    void SlamRunnerService::SaveResult() {
        FinalizeResultFiles();

        EmitLog(String::Format("Run id: {0}", _runId));
        EmitLog(String::Format("Final state: {0}", _state.ToString()));
        EmitLog(String::Format("Processed frames: {0}/{1}", _processedCount, _pcdTotalCount));
        if (_runStopwatch != nullptr &&
            _runStopwatch->Elapsed.TotalSeconds > 0.0 &&
            _processedCount > 0) {
            double averageFps = (double)_processedCount / _runStopwatch->Elapsed.TotalSeconds;
            EmitLog(String::Format("Average FPS: {0:F2} frames/s", averageFps));
        }

        if (!String::IsNullOrWhiteSpace(_runOutputDirectory)) {
            EmitLog(String::Format("Output directory: {0}", _runOutputDirectory));
        }
        if (!String::IsNullOrWhiteSpace(_logFilePath)) {
            EmitLog(String::Format("Runner log file: {0}", _logFilePath));
        }
    }

    void SlamRunnerService::RunWorker() {
        try {
            _resolvedCoreExePath = ResolveCoreExePath();
            _resolvedCoreDirectory = Path::GetDirectoryName(_resolvedCoreExePath);
            _pcdTotalCount = ValidatePcdDirectory(_request->PcdDirectory);
            PrepareRunPaths(_resolvedCoreDirectory);

            EmitLog(String::Format("Found {0} PCD files.", _pcdTotalCount));
            EmitLog(String::Format("Core exe: {0}", _resolvedCoreExePath));
            EmitLog(String::Format("Core working directory: {0}", _resolvedCoreDirectory));
            EmitLog(String::Format("Runtime config: {0}", _runtimeConfigPath));
            EmitLog(String::Format("PCD directory: {0}", _request->PcdDirectory));

            if (_stopRequested) {
                SetState(SlamRunState::Stopped);
                EmitLog("SLAM task stopped before core process started.");
                return;
            }

            _process = gcnew Process();
            _process->StartInfo->FileName = _resolvedCoreExePath;
            _process->StartInfo->Arguments =
                "\"" + _runtimeConfigPath + "\" \"" + _request->PcdDirectory + "\"";
            _process->StartInfo->WorkingDirectory = _resolvedCoreDirectory;
            _process->StartInfo->UseShellExecute = false;
            _process->StartInfo->RedirectStandardOutput = true;
            _process->StartInfo->RedirectStandardError = true;
            _process->StartInfo->CreateNoWindow = true;

            _process->OutputDataReceived += gcnew DataReceivedEventHandler(
                this, &SlamRunnerService::OnOutputDataReceived);
            _process->ErrorDataReceived += gcnew DataReceivedEventHandler(
                this, &SlamRunnerService::OnErrorDataReceived);

            SetState(SlamRunState::Running);

            _runStopwatch = Stopwatch::StartNew();
            _process->Start();
            _process->BeginOutputReadLine();
            _process->BeginErrorReadLine();
            _process->WaitForExit();
            _process->WaitForExit(1000);
            if (_runStopwatch != nullptr) {
                _runStopwatch->Stop();
            }

            int exitCode = _process->ExitCode;

            if (_stopRequested) {
                SetState(SlamRunState::Stopped);
                EmitLog("SLAM task stopped.");
            }
            else if (exitCode == 0) {
                _processedCount = _pcdTotalCount;
                EmitFrameProgress(_processedCount, true);
                SetState(SlamRunState::Completed);
                EmitLog("SLAM task completed.");
            }
            else {
                SetState(SlamRunState::Failed);
                EmitLog(String::Format("SLAM task failed, exit code: {0}. {1}",
                    exitCode, DescribeCoreExitCode(exitCode)));
            }

            SaveResult();
        }
        catch (Exception^ ex) {
            if (_runStopwatch != nullptr && _runStopwatch->IsRunning) {
                _runStopwatch->Stop();
            }
            SetState(_stopRequested ? SlamRunState::Stopped : SlamRunState::Failed);
            EmitLog(String::Format("Error: {0}", ex->Message));
            SaveResult();
        }
        finally {
            Cleanup();
            EmitCompleted();
        }
    }

    void SlamRunnerService::OnOutputDataReceived(Object^ sender, DataReceivedEventArgs^ e) {
        if (e->Data == nullptr) return;
        HandleCoreOutputLine(e->Data);
    }

    void SlamRunnerService::OnErrorDataReceived(Object^ sender, DataReceivedEventArgs^ e) {
        if (e->Data == nullptr) return;
        HandleCoreOutputLine("[stderr] " + e->Data);
    }

    int SlamRunnerService::CountPcdFiles(String^ directory) {
        if (String::IsNullOrWhiteSpace(directory) || !Directory::Exists(directory)) {
            return 0;
        }

        int count = 0;
        array<String^>^ files = Directory::GetFiles(directory, "*", SearchOption::TopDirectoryOnly);
        for each (String^ file in files) {
            if (String::Equals(Path::GetExtension(file), ".pcd",
                StringComparison::OrdinalIgnoreCase)) {
                ++count;
            }
        }
        return count;
    }

    int SlamRunnerService::ValidatePcdDirectory(String^ directory) {
        if (String::IsNullOrWhiteSpace(directory)) {
            throw gcnew ArgumentException("PCD directory is empty.");
        }

        String^ fullPath = Path::GetFullPath(directory);
        if (File::Exists(fullPath)) {
            throw gcnew IOException(
                String::Format("Input path is not a directory: {0}", fullPath));
        }
        if (!Directory::Exists(fullPath)) {
            throw gcnew DirectoryNotFoundException(
                String::Format("Input directory does not exist: {0}", fullPath));
        }

        array<String^>^ entries = Directory::GetFileSystemEntries(fullPath);
        if (entries->Length == 0) {
            throw gcnew InvalidOperationException(
                String::Format("Input directory is empty: {0}", fullPath));
        }

        int pcdCount = CountPcdFiles(fullPath);
        if (pcdCount <= 0) {
            array<String^>^ files = Directory::GetFiles(fullPath, "*", SearchOption::TopDirectoryOnly);
            if (files->Length == 1) {
                throw gcnew InvalidOperationException(
                    String::Format("File format error: expected .pcd, got {0}",
                        Path::GetFileName(files[0])));
            }

            throw gcnew InvalidOperationException(
                String::Format("No PCD files found in directory: {0}", fullPath));
        }

        return pcdCount;
    }

    void SlamRunnerService::EnsureDirectoryWritable(String^ directory, String^ description) {
        if (String::IsNullOrWhiteSpace(directory)) {
            throw gcnew ArgumentException(description + " path is empty.");
        }

        String^ fullPath = Path::GetFullPath(directory);
        if (File::Exists(fullPath)) {
            throw gcnew IOException(
                String::Format("{0} path is a file, not a directory: {1}",
                    description, fullPath));
        }

        Directory::CreateDirectory(fullPath);

        String^ probePath = Path::Combine(
            fullPath, ".slamrunner_write_test_" + Guid::NewGuid().ToString("N") + ".tmp");
        try {
            File::WriteAllText(probePath, "ok", Encoding::UTF8);
        }
        catch (Exception^ ex) {
            throw gcnew UnauthorizedAccessException(
                String::Format("{0} is not writable: {1}. {2}",
                    description, fullPath, ex->Message), ex);
        }
        finally {
            try {
                if (File::Exists(probePath)) {
                    File::Delete(probePath);
                }
            }
            catch (Exception^) {}
        }
    }

    String^ SlamRunnerService::DescribeCoreExitCode(int exitCode) {
        switch (exitCode) {
        case 10:
            return "Input data validation failed. Please check the PCD directory.";
        case 20:
            return "Configuration validation failed. Please check the YAML file.";
        case 30:
            return "Output path permission check failed. Please check the output directory.";
        case 40:
            return "Core runtime error. Please check the core log for details.";
        default:
            return "Unknown core error. Please check the core log for details.";
        }
    }

    void SlamRunnerService::ParseProgress(String^ line) {
        if (String::IsNullOrEmpty(line) || !line->Contains("frame_id =")) {
            return;
        }

        try {
            int idx = line->IndexOf("frame_id =");
            String^ sub = line->Substring(idx + 10)->Trim();
            int commaIdx = sub->IndexOf(",");
            if (commaIdx > 0) {
                sub = sub->Substring(0, commaIdx);
            }

            int frameId = Int32::Parse(sub->Trim());
            _processedCount = frameId;
            EmitFrameProgress(frameId, false);
        }
        catch (Exception^) {
            // Ignore progress parse errors. The raw line remains in the runner log file.
        }
    }

    void SlamRunnerService::EmitFrameProgress(int frameId, bool force) {
        if (frameId <= 0) {
            return;
        }

        bool isFinalFrame = (_pcdTotalCount > 0 && frameId >= _pcdTotalCount);
        bool shouldEmit = force || isFinalFrame;
        if (!shouldEmit && _uiFrameLogInterval > 0) {
            shouldEmit = (frameId % _uiFrameLogInterval) == 0;
        }
        if (!shouldEmit || frameId == _lastUiLogFrame) {
            return;
        }

        if (_pcdTotalCount > 0) {
            int pct = (int)((double)frameId / _pcdTotalCount * 100.0);
            if (pct < 0) pct = 0;
            if (pct > 100) pct = 100;
            EmitProgress(pct);
        }

        String^ totalText = _pcdTotalCount > 0 ? _pcdTotalCount.ToString() : "?";
        EmitUiLog(String::Format("Completed frames: {0}/{1}", frameId, totalText));
        _lastUiLogFrame = frameId;
    }

    void SlamRunnerService::HandleCoreOutputLine(String^ message) {
        if (message == nullptr) {
            return;
        }

        WriteLogFile(message);
        ParseProgress(message);
    }

    void SlamRunnerService::PrepareRunPaths(String^ coreDirectory) {
        _runId = DateTime::Now.ToString("yyyyMMdd_HHmmss");

        String^ guiRoot = GetGuiRootDirectory();
        String^ outputRoot = _request->OutputDirectory;
        if (String::IsNullOrWhiteSpace(outputRoot)) {
            outputRoot = Path::Combine(guiRoot, "runtime", "output");
        }
        outputRoot = Path::GetFullPath(outputRoot);

        String^ logsRoot = Path::Combine(guiRoot, "runtime", "logs");
        _runOutputDirectory = Path::Combine(outputRoot, _runId);
        _runLogDirectory = Path::Combine(logsRoot, _runId);

        EnsureDirectoryWritable(outputRoot, "Output root directory");
        EnsureDirectoryWritable(_runOutputDirectory, "Run output directory");
        EnsureDirectoryWritable(logsRoot, "Log root directory");
        EnsureDirectoryWritable(_runLogDirectory, "Run log directory");

        _logFilePath = Path::Combine(logsRoot, "log_" + _runId + ".txt");
        _mapOutputPath = Path::Combine(_runOutputDirectory, "map_" + _runId + ".pcd");
        _trajectoryOutputPath = Path::Combine(_runOutputDirectory, "trajectory_" + _runId + ".txt");

        _logWriter = gcnew StreamWriter(_logFilePath, false, gcnew UTF8Encoding(false));
        _logWriter->AutoFlush = true;

        _runtimeConfigPath = CreateRuntimeConfig(_request->ConfigPath);
    }

    String^ SlamRunnerService::ResolveCoreExePath() {
        if (!String::IsNullOrWhiteSpace(CoreExePath)) {
            String^ fullPath = Path::GetFullPath(CoreExePath);
            if (!File::Exists(fullPath)) {
                throw gcnew FileNotFoundException(
                    String::Format("Core exe not found: {0}", fullPath));
            }
            return fullPath;
        }

        String^ baseDir = Path::GetDirectoryName(
            System::Reflection::Assembly::GetExecutingAssembly()->Location);
        array<String^>^ candidates = gcnew array<String^> {
            Path::Combine(baseDir, "core", "oh_my_loam_noros.exe"),
            Path::Combine(baseDir, "runtime", "core", "oh_my_loam_noros.exe"),
            Path::Combine(baseDir, "..", "..", "runtime", "core", "oh_my_loam_noros.exe"),
            Path::Combine(Directory::GetCurrentDirectory(), "runtime", "core", "oh_my_loam_noros.exe"),
            Path::Combine(Directory::GetCurrentDirectory(), "gui", "runtime", "core", "oh_my_loam_noros.exe")
        };

        for each (String^ candidate in candidates) {
            String^ fullPath = Path::GetFullPath(candidate);
            if (File::Exists(fullPath)) {
                return fullPath;
            }
        }

        throw gcnew FileNotFoundException(
            "Core exe not found. Expected oh_my_loam_noros.exe under core/ or runtime/core/.");
    }

    String^ SlamRunnerService::CreateRuntimeConfig(String^ sourceConfigPath) {
        if (String::IsNullOrWhiteSpace(sourceConfigPath) || !File::Exists(sourceConfigPath)) {
            throw gcnew FileNotFoundException(
                String::Format("Config file not found: {0}", sourceConfigPath));
        }

        array<String^>^ lines = nullptr;
        try {
            lines = File::ReadAllLines(sourceConfigPath, Encoding::UTF8);
        }
        catch (Exception^ ex) {
            throw gcnew IOException(
                String::Format("Failed to read config file: {0}. {1}",
                    sourceConfigPath, ex->Message), ex);
        }

        bool hasLogPath = false;
        bool hasSaveMapPath = false;
        bool hasGuiLogFrameInterval = false;
        _uiFrameLogInterval = 50;
        String^ guiLogFrameIntervalKey = "gui_log_frame_interval:";
        List<String^>^ updatedLines = gcnew List<String^>();
        for (int i = 0; i < lines->Length; ++i) {
            String^ line = lines[i];
            String^ trimmed = line->TrimStart();
            String^ indent = line->Substring(0, line->Length - trimmed->Length);

            if (trimmed->StartsWith("log_path:", StringComparison::OrdinalIgnoreCase)) {
                updatedLines->Add(indent + "log_path: " + QuoteYamlPath(_runLogDirectory));
                hasLogPath = true;
            }
            else if (trimmed->StartsWith("save_map_path:", StringComparison::OrdinalIgnoreCase)) {
                updatedLines->Add(indent + "save_map_path: " + QuoteYamlPath(_mapOutputPath));
                hasSaveMapPath = true;
            }
            else if (trimmed->StartsWith(guiLogFrameIntervalKey, StringComparison::OrdinalIgnoreCase)) {
                String^ rawValue = trimmed->Substring(guiLogFrameIntervalKey->Length)->Trim();
                int commentIndex = rawValue->IndexOf("#");
                if (commentIndex >= 0) {
                    rawValue = rawValue->Substring(0, commentIndex)->Trim();
                }

                int parsedInterval = 0;
                if (Int32::TryParse(rawValue, parsedInterval) && parsedInterval > 0) {
                    _uiFrameLogInterval = parsedInterval;
                }
                hasGuiLogFrameInterval = true;
                updatedLines->Add(line);
            }
            else {
                updatedLines->Add(line);
            }
        }

        if (!hasLogPath) {
            updatedLines->Add("log_path: " + QuoteYamlPath(_runLogDirectory));
        }
        if (!hasSaveMapPath) {
            updatedLines->Add("save_map_path: " + QuoteYamlPath(_mapOutputPath));
        }
        if (!hasGuiLogFrameInterval) {
            updatedLines->Add("gui_log_frame_interval: 50");
        }

        String^ configPath = Path::Combine(_runOutputDirectory, "config_" + _runId + ".yaml");
        try {
            File::WriteAllLines(configPath, updatedLines->ToArray(), gcnew UTF8Encoding(false));
        }
        catch (Exception^ ex) {
            throw gcnew IOException(
                String::Format("Failed to write runtime config: {0}. {1}",
                    configPath, ex->Message), ex);
        }
        return configPath;
    }

    String^ SlamRunnerService::QuoteYamlPath(String^ path) {
        return "\"" + Path::GetFullPath(path)->Replace("\\", "/") + "\"";
    }

    String^ SlamRunnerService::GetGuiRootDirectory() {
        String^ baseDir = Path::GetDirectoryName(
            System::Reflection::Assembly::GetExecutingAssembly()->Location);
        baseDir = Path::GetFullPath(baseDir);
        DirectoryInfo^ dir = gcnew DirectoryInfo(baseDir);

        while (dir != nullptr) {
            if (Directory::Exists(Path::Combine(dir->FullName, "runtime", "core"))) {
                return dir->FullName;
            }
            dir = dir->Parent;
        }

        return Path::GetFullPath(Path::Combine(baseDir, "..", ".."));
    }

    void SlamRunnerService::FinalizeResultFiles() {
        if (String::IsNullOrWhiteSpace(_runOutputDirectory)) {
            return;
        }

        try {
            String^ defaultTrajectory = Path::Combine(_runOutputDirectory, "trajectory.txt");
            if (File::Exists(defaultTrajectory) && !String::IsNullOrWhiteSpace(_trajectoryOutputPath)) {
                if (File::Exists(_trajectoryOutputPath)) {
                    File::Delete(_trajectoryOutputPath);
                }
                File::Move(defaultTrajectory, _trajectoryOutputPath);
                EmitLog(String::Format("Trajectory renamed to: {0}", _trajectoryOutputPath));
            }

            if (!String::IsNullOrWhiteSpace(_mapOutputPath) && File::Exists(_mapOutputPath)) {
                EmitLog(String::Format("Map output: {0}", _mapOutputPath));
            }
        }
        catch (Exception^ ex) {
            EmitLog(String::Format("Warning while finalizing result files: {0}", ex->Message));
        }
    }

    void SlamRunnerService::SetState(SlamRunState newState) {
        _state = newState;
        try {
            OnStateChanged(this, gcnew StateChangedEventArgs(newState));
        }
        catch (NullReferenceException^) {}
    }

    void SlamRunnerService::EmitLog(String^ message) {
        if (message == nullptr) {
            return;
        }

        WriteLogFile(message);
        EmitUiLog(message);
    }

    void SlamRunnerService::WriteLogFile(String^ message) {
        if (message == nullptr) {
            return;
        }

        try {
            if (_logWriter != nullptr) {
                _logWriter->WriteLine(
                    DateTime::Now.ToString("yyyy-MM-dd HH:mm:ss.fff") + " " + message);
            }
        }
        catch (Exception^) {
            // Logging to file must never break the runner.
        }
    }

    void SlamRunnerService::EmitUiLog(String^ message) {
        if (message == nullptr) {
            return;
        }

        try {
            OnLog(this, gcnew LogEventArgs(message));
        }
        catch (NullReferenceException^) {}
    }

    void SlamRunnerService::EmitProgress(int percent) {
        if (percent < 0) percent = 0;
        if (percent > 100) percent = 100;

        try {
            OnProgress(this, gcnew ProgressEventArgs(percent));
        }
        catch (NullReferenceException^) {}
    }

    void SlamRunnerService::EmitCompleted() {
        try {
            OnCompleted(this, EventArgs::Empty);
        }
        catch (NullReferenceException^) {}
    }

    void SlamRunnerService::CloseLogWriter() {
        try {
            if (_logWriter != nullptr) {
                _logWriter->Flush();
                _logWriter->Close();
            }
        }
        catch (Exception^) {}
        _logWriter = nullptr;
    }

    void SlamRunnerService::Cleanup() {
        try {
            if (_process != nullptr) {
                if (!_process->HasExited) {
                    _process->Kill();
                }
                _process->Close();
            }
        }
        catch (Exception^) {}

        _process = nullptr;
        _runThread = nullptr;
        CloseLogWriter();
    }
}
