#pragma once

namespace SlamGui {

    using namespace System;
    using namespace System::Windows::Forms;
    using namespace System::Drawing;
    using namespace SlamRunner;

    public ref class FormMain : public Form {
    public:
        FormMain();

    private:
        Label^ lblDataPath;
        Label^ lblConfigPath;
        Label^ lblOutputPath;
        TextBox^ txtDataPath;
        Button^ btnBrowseData;
        TextBox^ txtConfigPath;
        Button^ btnBrowseConfig;
        TextBox^ txtOutputPath;
        Button^ btnStart;
        Button^ btnStop;
        RichTextBox^ rtbLog;
        ProgressBar^ progressBar;
        Label^ lblStatus;

        SlamRunnerService^ _runner;

        void InitializeComponent();
        void btnBrowseData_Click(Object^ sender, EventArgs^ e);
        void btnBrowseConfig_Click(Object^ sender, EventArgs^ e);
        void btnStart_Click(Object^ sender, EventArgs^ e);
        void btnStop_Click(Object^ sender, EventArgs^ e);
        void FormMain_FormClosing(Object^ sender, FormClosingEventArgs^ e);

        void OnRunnerLog(Object^ sender, LogEventArgs^ e);
        void OnRunnerProgress(Object^ sender, ProgressEventArgs^ e);
        void OnRunnerStateChanged(Object^ sender, StateChangedEventArgs^ e);
        void OnRunnerCompleted(Object^ sender, EventArgs^ e);

        void AppendLog(String^ message);
        void SetRunningState(bool running);
        void InitializeDefaultPaths();
        String^ FindGuiRootDirectory();
    };
}
