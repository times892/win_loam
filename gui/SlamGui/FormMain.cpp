#pragma execution_character_set("utf-8")
#include "FormMain.h"

namespace SlamGui {

    FormMain::FormMain() {
        InitializeComponent();
        InitializeDefaultPaths();
    }

    void FormMain::InitializeComponent() {
        this->SuspendLayout();

        this->Text = "中原工win版slam";
        this->Size = System::Drawing::Size(900, 650);
        this->MinimumSize = System::Drawing::Size(780, 540);
        this->StartPosition = FormStartPosition::CenterScreen;
        this->Font = gcnew System::Drawing::Font("Microsoft YaHei UI", 9);
        this->FormClosing += gcnew FormClosingEventHandler(this, &FormMain::FormMain_FormClosing);

        lblStatus = gcnew Label();
        lblStatus->Text = "就绪";
        lblStatus->Location = System::Drawing::Point(12, 15);
        lblStatus->AutoSize = true;

        lblDataPath = gcnew Label();
        lblDataPath->Text = "点云目录：";
        lblDataPath->Location = System::Drawing::Point(12, 48);
        lblDataPath->AutoSize = true;

        txtDataPath = gcnew TextBox();
        txtDataPath->Location = System::Drawing::Point(100, 45);
        txtDataPath->Size = System::Drawing::Size(560, 23);
        txtDataPath->Anchor = AnchorStyles::Top | AnchorStyles::Left | AnchorStyles::Right;

        btnBrowseData = gcnew Button();
        btnBrowseData->Text = "浏览...";
        btnBrowseData->Location = System::Drawing::Point(670, 44);
        btnBrowseData->Size = System::Drawing::Size(80, 25);
        btnBrowseData->Anchor = AnchorStyles::Top | AnchorStyles::Right;
        btnBrowseData->Click += gcnew EventHandler(this, &FormMain::btnBrowseData_Click);

        lblConfigPath = gcnew Label();
        lblConfigPath->Text = "配置文件：";
        lblConfigPath->Location = System::Drawing::Point(12, 83);
        lblConfigPath->AutoSize = true;

        txtConfigPath = gcnew TextBox();
        txtConfigPath->Location = System::Drawing::Point(100, 80);
        txtConfigPath->Size = System::Drawing::Size(560, 23);
        txtConfigPath->Anchor = AnchorStyles::Top | AnchorStyles::Left | AnchorStyles::Right;

        btnBrowseConfig = gcnew Button();
        btnBrowseConfig->Text = "选择...";
        btnBrowseConfig->Location = System::Drawing::Point(670, 79);
        btnBrowseConfig->Size = System::Drawing::Size(80, 25);
        btnBrowseConfig->Anchor = AnchorStyles::Top | AnchorStyles::Right;
        btnBrowseConfig->Click += gcnew EventHandler(this, &FormMain::btnBrowseConfig_Click);

        lblOutputPath = gcnew Label();
        lblOutputPath->Text = "输出目录：";
        lblOutputPath->Location = System::Drawing::Point(12, 118);
        lblOutputPath->AutoSize = true;

        txtOutputPath = gcnew TextBox();
        txtOutputPath->Location = System::Drawing::Point(100, 115);
        txtOutputPath->Size = System::Drawing::Size(560, 23);
        txtOutputPath->Anchor = AnchorStyles::Top | AnchorStyles::Left | AnchorStyles::Right;

        btnStart = gcnew Button();
        btnStart->Text = "开始 SLAM";
        btnStart->Location = System::Drawing::Point(100, 155);
        btnStart->Size = System::Drawing::Size(120, 35);
        btnStart->Click += gcnew EventHandler(this, &FormMain::btnStart_Click);

        btnStop = gcnew Button();
        btnStop->Text = "停止";
        btnStop->Location = System::Drawing::Point(230, 155);
        btnStop->Size = System::Drawing::Size(80, 35);
        btnStop->Enabled = false;
        btnStop->Click += gcnew EventHandler(this, &FormMain::btnStop_Click);

        progressBar = gcnew ProgressBar();
        progressBar->Location = System::Drawing::Point(12, 205);
        progressBar->Size = System::Drawing::Size(860, 20);
        progressBar->Anchor = AnchorStyles::Top | AnchorStyles::Left | AnchorStyles::Right;

        rtbLog = gcnew RichTextBox();
        rtbLog->Location = System::Drawing::Point(12, 235);
        rtbLog->Size = System::Drawing::Size(860, 360);
        rtbLog->Anchor = AnchorStyles::Top | AnchorStyles::Bottom | AnchorStyles::Left | AnchorStyles::Right;
        rtbLog->ReadOnly = true;
        rtbLog->BackColor = Color::FromArgb(30, 30, 30);
        rtbLog->ForeColor = Color::FromArgb(220, 220, 220);
        rtbLog->Font = gcnew System::Drawing::Font("Consolas", 9);

        this->Controls->Add(lblStatus);
        this->Controls->Add(lblDataPath);
        this->Controls->Add(txtDataPath);
        this->Controls->Add(btnBrowseData);
        this->Controls->Add(lblConfigPath);
        this->Controls->Add(txtConfigPath);
        this->Controls->Add(btnBrowseConfig);
        this->Controls->Add(lblOutputPath);
        this->Controls->Add(txtOutputPath);
        this->Controls->Add(btnStart);
        this->Controls->Add(btnStop);
        this->Controls->Add(progressBar);
        this->Controls->Add(rtbLog);

        this->ResumeLayout(false);
    }

    void FormMain::btnBrowseData_Click(Object^ sender, EventArgs^ e) {
        FolderBrowserDialog^ dlg = gcnew FolderBrowserDialog();
        dlg->Description = "选择 PCD 点云数据目录";
        if (dlg->ShowDialog() == System::Windows::Forms::DialogResult::OK) {
            txtDataPath->Text = dlg->SelectedPath;
        }
    }

    void FormMain::btnBrowseConfig_Click(Object^ sender, EventArgs^ e) {
        OpenFileDialog^ dlg = gcnew OpenFileDialog();
        dlg->Filter = "YAML 配置文件 (*.yaml;*.yml)|*.yaml;*.yml|所有文件 (*.*)|*.*";
        dlg->Title = "选择 YAML 配置文件";
        if (dlg->ShowDialog() == System::Windows::Forms::DialogResult::OK) {
            txtConfigPath->Text = dlg->FileName;
        }
    }

    void FormMain::btnStart_Click(Object^ sender, EventArgs^ e) {
        if (String::IsNullOrWhiteSpace(txtDataPath->Text)) {
            MessageBox::Show("请先选择 PCD 点云数据目录。", "提示",
                MessageBoxButtons::OK, MessageBoxIcon::Warning);
            return;
        }
        if (String::IsNullOrWhiteSpace(txtConfigPath->Text)) {
            MessageBox::Show("请先选择 YAML 配置文件。", "提示",
                MessageBoxButtons::OK, MessageBoxIcon::Warning);
            return;
        }

        try {
            _runner = gcnew SlamRunnerService();
            _runner->OnLog += gcnew LogEventHandler(this, &FormMain::OnRunnerLog);
            _runner->OnProgress += gcnew ProgressEventHandler(this, &FormMain::OnRunnerProgress);
            _runner->OnStateChanged += gcnew StateChangedEventHandler(this, &FormMain::OnRunnerStateChanged);
            _runner->OnCompleted += gcnew EventHandler(this, &FormMain::OnRunnerCompleted);

            _runner->LoadPointCloud(txtDataPath->Text);
            _runner->LoadConfig(txtConfigPath->Text);

            if (!String::IsNullOrWhiteSpace(txtOutputPath->Text)) {
                _runner->OutputDirectory = txtOutputPath->Text;
            }

            SetRunningState(true);
            lblStatus->Text = "准备启动...";
            progressBar->Value = 0;
            AppendLog("开始新的 SLAM 任务。");
            _runner->Run();
        }
        catch (Exception^ ex) {
            AppendLog("启动失败: " + ex->Message);
            if (_runner != nullptr) {
                _runner->OnLog -= gcnew LogEventHandler(this, &FormMain::OnRunnerLog);
                _runner->OnProgress -= gcnew ProgressEventHandler(this, &FormMain::OnRunnerProgress);
                _runner->OnStateChanged -= gcnew StateChangedEventHandler(this, &FormMain::OnRunnerStateChanged);
                _runner->OnCompleted -= gcnew EventHandler(this, &FormMain::OnRunnerCompleted);
            }
            SetRunningState(false);
            _runner = nullptr;
        }
    }

    void FormMain::btnStop_Click(Object^ sender, EventArgs^ e) {
        if (_runner != nullptr) {
            AppendLog("正在停止...");
            _runner->Stop();
        }
    }

    void FormMain::FormMain_FormClosing(Object^ sender, FormClosingEventArgs^ e) {
        if (_runner == nullptr) {
            return;
        }

        SlamRunState state = _runner->State;
        if (state == SlamRunState::Running || state == SlamRunState::Loading || state == SlamRunState::Stopping) {
            System::Windows::Forms::DialogResult result = MessageBox::Show(
                "SLAM 任务仍在运行。是否停止任务并关闭窗口？",
                "确认关闭",
                MessageBoxButtons::YesNo,
                MessageBoxIcon::Question);

            if (result != System::Windows::Forms::DialogResult::Yes) {
                e->Cancel = true;
                return;
            }

            _runner->Stop();
        }
    }

    void FormMain::OnRunnerLog(Object^ sender, LogEventArgs^ e) {
        if (this->InvokeRequired) {
            this->BeginInvoke(gcnew LogEventHandler(this, &FormMain::OnRunnerLog),
                gcnew array<Object^>{ sender, e });
            return;
        }
        AppendLog(e->Message);
    }

    void FormMain::OnRunnerProgress(Object^ sender, ProgressEventArgs^ e) {
        if (this->InvokeRequired) {
            this->BeginInvoke(gcnew ProgressEventHandler(this, &FormMain::OnRunnerProgress),
                gcnew array<Object^>{ sender, e });
            return;
        }
        if (e->Percent >= 0 && e->Percent <= 100) {
            progressBar->Value = e->Percent;
            if (_runner != nullptr && _runner->State == SlamRunState::Running) {
                lblStatus->Text = String::Format("运行中... {0}%", e->Percent);
            }
        }
    }

    void FormMain::OnRunnerStateChanged(Object^ sender, StateChangedEventArgs^ e) {
        if (this->InvokeRequired) {
            this->BeginInvoke(gcnew StateChangedEventHandler(this, &FormMain::OnRunnerStateChanged),
                gcnew array<Object^>{ sender, e });
            return;
        }
        switch (e->NewState) {
            case SlamRunState::Loading:
                lblStatus->Text = "加载中...";
                break;
            case SlamRunState::Running:
                lblStatus->Text = "运行中...";
                break;
            case SlamRunState::Stopping:
                lblStatus->Text = "正在停止...";
                break;
            case SlamRunState::Completed:
                lblStatus->Text = "已完成";
                break;
            case SlamRunState::Failed:
                lblStatus->Text = "失败";
                break;
            case SlamRunState::Stopped:
                lblStatus->Text = "已停止";
                break;
            default:
                lblStatus->Text = "就绪";
                break;
        }
    }

    void FormMain::OnRunnerCompleted(Object^ sender, EventArgs^ e) {
        if (this->InvokeRequired) {
            this->BeginInvoke(gcnew EventHandler(this, &FormMain::OnRunnerCompleted),
                gcnew array<Object^>{ sender, e });
            return;
        }
        if (_runner != nullptr) {
            if (!String::IsNullOrWhiteSpace(_runner->LastRunOutputDirectory)) {
                AppendLog("本次输出目录: " + _runner->LastRunOutputDirectory);
            }
            if (!String::IsNullOrWhiteSpace(_runner->LastLogFilePath)) {
                AppendLog("本次日志文件: " + _runner->LastLogFilePath);
            }
            _runner->OnLog -= gcnew LogEventHandler(this, &FormMain::OnRunnerLog);
            _runner->OnProgress -= gcnew ProgressEventHandler(this, &FormMain::OnRunnerProgress);
            _runner->OnStateChanged -= gcnew StateChangedEventHandler(this, &FormMain::OnRunnerStateChanged);
            _runner->OnCompleted -= gcnew EventHandler(this, &FormMain::OnRunnerCompleted);
            _runner = nullptr;
        }
        SetRunningState(false);
    }

    void FormMain::AppendLog(String^ message) {
        rtbLog->AppendText(DateTime::Now.ToString("HH:mm:ss") + "  " + message + "\r\n");
        rtbLog->SelectionStart = rtbLog->TextLength;
        rtbLog->ScrollToCaret();
    }

    void FormMain::SetRunningState(bool running) {
        btnStart->Enabled = !running;
        btnStop->Enabled = running;
        txtDataPath->Enabled = !running;
        btnBrowseData->Enabled = !running;
        txtConfigPath->Enabled = !running;
        btnBrowseConfig->Enabled = !running;
        txtOutputPath->Enabled = !running;
    }

    void FormMain::InitializeDefaultPaths() {
        String^ guiRoot = FindGuiRootDirectory();

        String^ defaultDataPath = System::IO::Path::Combine(guiRoot, "samples");
        if (System::IO::Directory::Exists(defaultDataPath)) {
            txtDataPath->Text = defaultDataPath;
        }

        String^ defaultConfigPath = System::IO::Path::Combine(
            guiRoot, "runtime", "core", "configs", "config_win10_noros.yaml");
        if (System::IO::File::Exists(defaultConfigPath)) {
            txtConfigPath->Text = defaultConfigPath;
        }

        txtOutputPath->Text = System::IO::Path::Combine(guiRoot, "runtime", "output");
    }

    String^ FormMain::FindGuiRootDirectory() {
        String^ baseDir = System::IO::Path::GetFullPath(AppDomain::CurrentDomain->BaseDirectory);
        System::IO::DirectoryInfo^ dir = gcnew System::IO::DirectoryInfo(baseDir);

        while (dir != nullptr) {
            if (System::IO::Directory::Exists(System::IO::Path::Combine(dir->FullName, "runtime", "core"))) {
                return dir->FullName;
            }
            dir = dir->Parent;
        }

        return System::IO::Path::GetFullPath(System::IO::Path::Combine(baseDir, "..", ".."));
    }
}
