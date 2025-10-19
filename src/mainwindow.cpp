#include "../include/mainwindow.h"
#include "../forms/ui_mainwindow.h"
#include "../include/speechrecognizer.h"
#include "../include/settingsmanager.h"
#include "../include/settingsdialog.h"
#include "../include/playbackwindow.h"
#include <QApplication>
#include <QMainWindow>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include <QProcess>
#include <QTimer>
#include <QFileInfo>
#include <QUrl>
#include <QDateTime>
#include <QMutex>
#include <QTextCodec>

// 全局消息处理器回调函数，将控制台输出重定向到UI
static void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // 获取MainWindow实例
    MainWindow* mainWindow = nullptr;
    for (QWidget* widget : QApplication::allWidgets()) {
        mainWindow = qobject_cast<MainWindow*>(widget);
        if (mainWindow) break;
    }
    
    if (!mainWindow) {
        // 如果MainWindow尚未创建，使用默认的消息处理
        static QTextStream ts(stdout);
        switch (type) {
        case QtDebugMsg:
            ts << "Debug: " << msg << " (" << context.file << ":" << context.line << ")" << endl;
            break;
        case QtInfoMsg:
            ts << "Info: " << msg << endl;
            break;
        case QtWarningMsg:
            ts << "Warning: " << msg << " (" << context.file << ":" << context.line << ")" << endl;
            break;
        case QtCriticalMsg:
            ts << "Critical: " << msg << " (" << context.file << ":" << context.line << ")" << endl;
            break;
        case QtFatalMsg:
            ts << "Fatal: " << msg << " (" << context.file << ":" << context.line << ")" << endl;
            abort();
        }
        return;
    }
    
    // 根据消息类型设置日志级别
    QString level;
    switch (type) {
    case QtDebugMsg:
        level = "DEBUG";
        break;
    case QtInfoMsg:
        level = "INFO";
        break;
    case QtWarningMsg:
        level = "WARNING";
        break;
    case QtCriticalMsg:
        level = "ERROR";
        break;
    case QtFatalMsg:
        level = "FATAL";
        break;
    default:
        level = "UNKNOWN";
    }
    
    // 提取文件名（去掉路径）
    QString fileName = QString(context.file).section('/', -1);
    
    // 如果消息包含[CRITICAL]前缀，确保使用ERROR级别
    QString displayMsg = msg;
    if (msg.contains("[CRITICAL]")) {
        level = "ERROR";
    }
    
    // 格式化消息，添加文件和行号信息（如果可用）
    if (context.file && context.line > 0) {
        displayMsg = QString("%1 (%2:%3)").arg(displayMsg).arg(fileName).arg(context.line);
    }
    
    // 使用MainWindow的logMessage方法将消息添加到UI
    mainWindow->logMessage(displayMsg, level);
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent),
                                          ui(new Ui::MainWindow),
                                          subtitleTimer(nullptr),
                                          m_speechRecognizer(nullptr),
                                          playbackWindow(nullptr),
                                          currentAudioFile(""),
                                          currentSubtitle(""),
                                          isRecognitionInProgress(false)
{
    // 安装自定义消息处理器，拦截所有qDebug、qInfo、qWarning、qCritical、qFatal输出
    qInstallMessageHandler(customMessageHandler);
    
    ui->setupUi(this);
    
    // 应用程序启动日志
    logMessage("EnPlayer启动成功", "SUCCESS");
    logMessage("欢迎使用EnPlayer语音识别", "INFO");
    logMessage("控制台日志已启用实时同步到UI", "INFO");
    
    // 立即检查FFmpeg可用性，在所有初始化之前
    checkFfmpegAvailability();
    
    initSubtitleTimer();
    initSpeechRecognition();

    // 连接设置变更信号
    connect(SettingsManager::instance(), &SettingsManager::settingsChanged, this, &MainWindow::onSettingsChanged);

    // 连接跳转到播放界面的按钮
    connect(ui->goToPlaybackButton, &QPushButton::clicked, this, &MainWindow::on_goToPlaybackButton_clicked);
}

MainWindow::~MainWindow()
{
    // 保存设置
    SettingsManager::instance()->saveSettings();

    // 停止语音识别
    if (m_speechRecognizer)
    {
        m_speechRecognizer->stop();
        delete m_speechRecognizer;
        m_speechRecognizer = nullptr;
    }

    // 关闭播放窗口（如果存在）
    if (playbackWindow) {
        playbackWindow->close();
        delete playbackWindow;
        playbackWindow = nullptr;
    }

    // 释放其他资源
    subtitleTimer->deleteLater();
    delete ui;
}

void MainWindow::initSpeechRecognition()
{
    // 初始化设置管理器
    SettingsManager::instance()->initialize();

    // 创建语音识别器实例
    m_speechRecognizer = new SpeechRecognizer(this);

    // 连接语音识别器的所有信号
    connect(m_speechRecognizer, &SpeechRecognizer::recognitionFinished, this, &MainWindow::onRecognitionFinished);
    connect(m_speechRecognizer, &SpeechRecognizer::recognitionError, this, &MainWindow::onRecognitionError);
    connect(m_speechRecognizer, &SpeechRecognizer::recognitionProgress, this, &MainWindow::onRecognitionProgress);

    // 初始化语音识别器
    bool initialized = m_speechRecognizer->initialize();
    if (!initialized)
    {
        ui->statusbar->showMessage(tr("语音识别器初始化失败，请检查Whisper模型路径"), 5000);
    }
}

void MainWindow::checkFfmpegAvailability()
{
    // 使用qDebug直接输出到控制台，确保能看到
    qDebug() << "[CRITICAL] 开始检查FFmpeg可用性...";
    
    QProcess ffmpegProcess;
    QStringList arguments;
    arguments << "-version";
    
    qDebug() << "[CRITICAL] 尝试执行FFmpeg命令: ffmpeg -version";
    ffmpegProcess.start("ffmpeg", arguments);
    
    // 等待进程启动并完成
    if (!ffmpegProcess.waitForStarted(2000)) {
        qCritical() << "[CRITICAL] 无法启动FFmpeg进程，可能是ffmpeg未安装或不在系统PATH中";
        qCritical() << "[CRITICAL] 系统PATH:" << qgetenv("PATH");
        logMessage("无法启动FFmpeg进程，可能是ffmpeg未安装或不在系统PATH中", "ERROR");
        logMessage("系统PATH: " + qgetenv("PATH"), "INFO");
        return;
    }
    
    if (!ffmpegProcess.waitForFinished(3000)) {
        qCritical() << "[CRITICAL] FFmpeg进程超时，可能存在问题";
        qCritical() << "[CRITICAL] FFmpeg标准输出:" << ffmpegProcess.readAllStandardOutput();
        qCritical() << "[CRITICAL] FFmpeg标准错误:" << ffmpegProcess.readAllStandardError();
        logMessage("FFmpeg进程超时，可能存在问题", "ERROR");
        logMessage("FFmpeg标准输出: " + ffmpegProcess.readAllStandardOutput(), "INFO");
        logMessage("FFmpeg标准错误: " + ffmpegProcess.readAllStandardError(), "INFO");
        return;
    }
    
    int exitCode = ffmpegProcess.exitCode();
    QString output = ffmpegProcess.readAllStandardOutput();
    QString error = ffmpegProcess.readAllStandardError();
    
    if (exitCode == 0) {
        qDebug() << "[CRITICAL] FFmpeg可用! 版本信息:" << output.left(100);
        logMessage("FFmpeg可用! 版本信息: " + output.left(100), "SUCCESS");
    } else {
        qCritical() << "[CRITICAL] FFmpeg执行失败，退出码:" << exitCode;
        qCritical() << "[CRITICAL] FFmpeg标准输出:" << output;
        qCritical() << "[CRITICAL] FFmpeg标准错误:" << error;
        logMessage(QString("FFmpeg执行失败，退出码: %1").arg(exitCode), "ERROR");
        logMessage("FFmpeg标准输出: " + output, "INFO");
        logMessage("FFmpeg标准错误: " + error, "INFO");
    }
}

void MainWindow::onSettingsChanged()
{
    // 重新初始化语音识别器以应用新设置
    if (m_speechRecognizer) {
        m_speechRecognizer->stop();
        delete m_speechRecognizer;
        m_speechRecognizer = nullptr;
    }
    initSpeechRecognition();
    logMessage("设置已更新，语音识别器已重新初始化", "INFO");
}

void MainWindow::on_openButton_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("打开视频/音频文件"), "", 
                        tr("媒体文件 (*.mp4 *.avi *.mov *.mkv *.mp3 *.wav *.flac)") + ";;" + tr("所有文件 (*)"));

    if (!fileName.isEmpty())
    {
        logMessage(QString("选择媒体文件: %1").arg(fileName), "INFO");
        
        // 保存当前媒体文件路径
        currentAudioFile = fileName;
        
        // 更新UI显示
        QFileInfo fileInfo(fileName);
        ui->currentFileLabel->setText(tr("当前文件: %1").arg(fileInfo.fileName()));
        ui->statusLabel->setText(tr("文件已加载，准备进行语音识别"));
        
        logMessage("媒体文件加载成功，准备进行语音识别", "SUCCESS");
        
        // 启用语音识别按钮
        ui->startRecognitionButton->setEnabled(true);
    }
    else
    {
        logMessage("用户取消了文件选择", "INFO");
    }
}

void MainWindow::on_startRecognitionButton_clicked()
{
    if (!currentAudioFile.isEmpty() && !isRecognitionInProgress)
    {
        // 直接调用startSpeechRecognition方法进行语音识别
        startSpeechRecognition();
    }
    else if (isRecognitionInProgress)
    {
        logMessage("识别任务已在进行中，请等待完成", "WARNING");
        ui->statusLabel->setText(tr("识别任务已在进行中，请等待完成"));
    }
    else
    {
        logMessage("请先选择一个媒体文件", "WARNING");
        ui->statusLabel->setText(tr("请先选择一个媒体文件"));
        QMessageBox::warning(this, tr("警告"), tr("请先选择一个媒体文件再进行语音识别"));
    }
}

// 跳转到播放界面的方法
void MainWindow::on_goToPlaybackButton_clicked()
{
    // 跳转到播放界面
    if (currentAudioFile.isEmpty()) {
        logMessage("请先选择音频文件", "ERROR");
        return;
    }
    
    if (!playbackWindow) {
        playbackWindow = new PlaybackWindow(this);
        connect(playbackWindow, &PlaybackWindow::backToRecognitionRequested, this, &MainWindow::onPlaybackWindowClosed);
        connect(playbackWindow, &QMainWindow::destroyed, this, &MainWindow::onPlaybackWindowClosed);
    }
    
    // 设置媒体文件路径
    playbackWindow->setMediaFilePath(currentAudioFile);
    playbackWindow->setSubtitleContent(currentSubtitle);
    
    // 隐藏主窗口，显示播放窗口
    this->hide();
    playbackWindow->show();
}

// 播放窗口关闭时的处理方法
void MainWindow::onPlaybackWindowClosed()
{
    logMessage("播放窗口已关闭", "INFO");
    this->show();
    
    // 清除播放窗口指针
    if (playbackWindow) {
        delete playbackWindow;
        playbackWindow = nullptr;
    }
}

// 删除不再需要的formatTime方法和processAudioForSubtitles方法

void MainWindow::onRecognitionFinished(const QString &text)
{
    // 保存识别结果
    currentSubtitle = text;
    
    // 显示识别结果
    ui->subtitleTextEdit->setText(text);
    ui->statusLabel->setText(tr("语音识别完成！"));
    
    // 添加日志记录
    logMessage("语音识别完成", "SUCCESS");
    logMessage(QString("识别文本长度: %1 字符").arg(text.length()), "INFO");
    
    // 识别完成，恢复状态标记
    isRecognitionInProgress = false;
    ui->startRecognitionButton->setEnabled(true);
    
    // 启用跳转到播放界面的按钮
    ui->goToPlaybackButton->setEnabled(true);
    
    // 显示提示信息
    showRecognitionCompletePrompt();
}

void MainWindow::onRecognitionError(const QString &errorMessage)
{
    qDebug() << "语音识别错误:" << errorMessage;
    
    // 清空字幕显示
    ui->subtitleTextEdit->clear();
    
    // 检查是否是Whisper未安装的情况
    if (errorMessage.contains("Whisper executable not found")) {
        ui->subtitleTextEdit->setPlaceholderText(tr("未找到Whisper可执行文件。请安装Whisper并在设置中配置路径。"));
        ui->statusLabel->setText(tr("未找到Whisper可执行文件"));
        qWarning() << "Whisper not found. Please install Whisper using: pip install openai-whisper";
        logMessage("未找到Whisper可执行文件", "ERROR");
    } else {
        ui->subtitleTextEdit->setPlaceholderText(tr("语音识别失败，请检查Whisper配置"));
        ui->statusLabel->setText(tr("语音识别失败"));
        logMessage(QString("识别错误: %1").arg(errorMessage), "ERROR");
    }
    
    // 识别出错，恢复状态标记
    isRecognitionInProgress = false;
    ui->startRecognitionButton->setEnabled(true);
}

void MainWindow::logMessage(const QString &message, const QString &level)
{
    // 获取当前时间
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    
    // 根据日志级别设置不同的颜色
    QString colorCode;
    if (level == "ERROR")
        colorCode = "#FF5555";
    else if (level == "WARNING")
        colorCode = "#FFAA00";
    else if (level == "SUCCESS")
        colorCode = "#00AA00";
    else if (level == "DEBUG")
        colorCode = "#5555FF";
    else
        colorCode = "#000000";
    
    // 格式化日志消息
    QString logEntry = QString("<font color='%1'>[%2] [%3] %4</font><br>")
                      .arg(colorCode)
                      .arg(timestamp)
                      .arg(level)
                      .arg(message);
    
    // 在UI线程中添加日志
    QMetaObject::invokeMethod(ui->logTextEdit, "append", Qt::QueuedConnection,
                            Q_ARG(QString, logEntry));
}

void MainWindow::on_clearLogButton_clicked()
{
    ui->logTextEdit->clear();
    logMessage("日志已清空", "INFO");
}

void MainWindow::onRecognitionProgress(int progress)
{
    // 更新进度条和状态显示
    ui->recognitionProgressBar->setValue(progress);
    ui->statusLabel->setText(tr("正在进行语音识别... ") + QString::number(progress) + "%");
    logMessage(QString("识别进度: %1%").arg(progress), "INFO");
}

// 启动语音识别
void MainWindow::startSpeechRecognition()
{
    if (currentAudioFile.isEmpty()) {
        logMessage("请先选择一个音频文件", "ERROR");
        return;
    }
    
    isRecognitionInProgress = true;
    ui->startRecognitionButton->setEnabled(false);
    ui->openButton->setEnabled(false);
    ui->recognitionProgressBar->setValue(0);
    ui->statusLabel->setText(tr("正在进行语音识别，请稍候..."));
    
    logMessage("开始语音识别处理...", "INFO");
    
    // 调用语音识别器进行识别
    m_speechRecognizer->recognizeFile(currentAudioFile);
}

// 初始化字幕计时器，保持用于兼容性但不再用于自动触发识别
void MainWindow::initSubtitleTimer()
{
    subtitleTimer = new QTimer(this);
    // 不再连接到processAudioForSubtitles，因为现在由用户手动触发识别
    subtitleTimer->setInterval(100); // 保留相同的间隔设置
    subtitleTimer->start();
}

// 处理音频获取字幕
void MainWindow::processAudioForSubtitles()
{
    // 此方法在新的设计中不再使用，但保留以避免编译错误
    // 识别现在通过startSpeechRecognition()方法手动触发
}

// 显示识别完成的提示信息
void MainWindow::showRecognitionCompletePrompt()
{
    QMessageBox::information(this, tr("语音识别完成"), 
                             tr("语音识别已成功完成！\n您现在可以点击\"前往音频播放界面\"按钮进行后续操作。"));
}

void MainWindow::on_actionSettings_triggered()
{
    SettingsDialog dialog(this);
    dialog.exec();

    // 重新初始化语音识别器以应用新设置
    if (m_speechRecognizer)
    {
        bool initialized = m_speechRecognizer->initialize();
        if (!initialized)
        {
            ui->statusLabel->setText(tr("语音识别器设置已更新，但初始化失败，请检查设置"));
            logMessage("语音识别器初始化失败", "ERROR");
        }
        else
        {
            ui->statusLabel->setText(tr("设置已应用"));
            logMessage("设置已应用", "INFO");
        }
    }
}
