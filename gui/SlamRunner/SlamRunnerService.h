#pragma once

using namespace System;
using namespace System::Collections::Generic;
using namespace System::Diagnostics;
using namespace System::IO;
using namespace System::Threading;

namespace SlamRunner {

    public enum class SlamRunState {
        Idle,
        Loading,
        Running,
        Stopping,
        Completed,
        Failed,
        Stopped
    };

    public ref class SlamRunRequest {
    public:
        property String^ PcdDirectory;
        property String^ ConfigPath;
        property String^ OutputDirectory;
    };

    public ref class LogEventArgs : public EventArgs {
    public:
        property String^ Message;
        LogEventArgs(String^ msg) { Message = msg; }
    };

    public ref class ProgressEventArgs : public EventArgs {
    public:
        property int Percent;
        ProgressEventArgs(int pct) { Percent = pct; }
    };

    public ref class StateChangedEventArgs : public EventArgs {
    public:
        property SlamRunState NewState;
        StateChangedEventArgs(SlamRunState state) { NewState = state; }
    };

    public delegate void LogEventHandler(Object^ sender, LogEventArgs^ e);
    public delegate void ProgressEventHandler(Object^ sender, ProgressEventArgs^ e);
    public delegate void StateChangedEventHandler(Object^ sender, StateChangedEventArgs^ e);

    public ref class SlamRunnerService {
    public:
        SlamRunnerService();
        ~SlamRunnerService();

        void LoadPointCloud(String^ pcdDirectory);
        void LoadConfig(String^ yamlPath);
        void Run();
        void Stop();
        void SaveResult();

        property SlamRunState State {
            SlamRunState get();
        }

        property String^ CoreExePath;
        property String^ WorkingDirectory;
        property String^ OutputDirectory {
            String^ get();
            void set(String^ value);
        }
        property String^ LastRunId {
            String^ get();
        }
        property String^ LastRunOutputDirectory {
            String^ get();
        }
        property String^ LastLogFilePath {
            String^ get();
        }

        event LogEventHandler^ OnLog;
        event ProgressEventHandler^ OnProgress;
        event StateChangedEventHandler^ OnStateChanged;
        event EventHandler^ OnCompleted;

    private:
        bool _hasRun;
        SlamRunState _state;
        SlamRunRequest^ _request;
        Process^ _process;
        Thread^ _runThread;
        StreamWriter^ _logWriter;
        Stopwatch^ _runStopwatch;
        volatile bool _stopRequested;
        int _pcdTotalCount;
        int _processedCount;
        int _uiFrameLogInterval;
        int _lastUiLogFrame;
        String^ _runId;
        String^ _resolvedCoreExePath;
        String^ _resolvedCoreDirectory;
        String^ _runtimeConfigPath;
        String^ _runOutputDirectory;
        String^ _runLogDirectory;
        String^ _logFilePath;
        String^ _mapOutputPath;
        String^ _trajectoryOutputPath;

        void SetState(SlamRunState newState);
        void RunWorker();
        void OnOutputDataReceived(Object^ sender, DataReceivedEventArgs^ e);
        void OnErrorDataReceived(Object^ sender, DataReceivedEventArgs^ e);
        int CountPcdFiles(String^ directory);
        int ValidatePcdDirectory(String^ directory);
        void EnsureDirectoryWritable(String^ directory, String^ description);
        String^ DescribeCoreExitCode(int exitCode);
        void ParseProgress(String^ line);
        void EmitFrameProgress(int frameId, bool force);
        void HandleCoreOutputLine(String^ message);
        void WriteLogFile(String^ message);
        void EmitUiLog(String^ message);
        void PrepareRunPaths(String^ coreDirectory);
        String^ ResolveCoreExePath();
        String^ CreateRuntimeConfig(String^ sourceConfigPath);
        String^ QuoteYamlPath(String^ path);
        String^ GetGuiRootDirectory();
        void FinalizeResultFiles();
        void EmitLog(String^ message);
        void EmitProgress(int percent);
        void EmitCompleted();
        void CloseLogWriter();
        void Cleanup();
    };
}
