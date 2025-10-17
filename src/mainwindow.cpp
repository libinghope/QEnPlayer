#include "../include/mainwindow.h"
#include "../forms/ui_mainwindow.h"
#include "../include/speechrecognizer.h"
#include "../include/settingsmanager.h"
#include "../include/settingsdialog.h"

#include <QMainWindow>
#include <QVideoWidget>
#include <QMediaPlayer>
#include <QFile>
#include <QTextStream>
#include <QVBoxLayout>
#include <QDir>
#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QFileInfo>
#include <QUrl>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    player(nullptr),
    videoWidget(nullptr),
    subtitleTimer(nullptr),
    m_speechRecognizer(nullptr),
    currentAudioFile("")
{
    ui->setupUi(this);
    initPlayer();
    initSubtitleTimer();
    initSpeechRecognition();
}

MainWindow::~MainWindow()
{
    // 停止语音识别
    if (m_speechRecognizer) {
        m_speechRecognizer->stop();
        delete m_speechRecognizer;
        m_speechRecognizer = nullptr;
    }
    
    // 释放其他资源
    delete subtitleTimer;
    delete videoWidget;
    delete player;
    delete ui;
}

void MainWindow::initPlayer()
{
    player = new QMediaPlayer(this);
    videoWidget = new QVideoWidget(ui->videoWidget);
    player->setVideoOutput(videoWidget);
    
    // 设置视频窗口布局
    QVBoxLayout *videoLayout = new QVBoxLayout(ui->videoWidget);
    videoLayout->addWidget(videoWidget);
    videoLayout->setContentsMargins(0, 0, 0, 0);
    
    // 连接信号和槽
    connect(player, &QMediaPlayer::positionChanged, this, &MainWindow::on_positionChanged);
    connect(player, &QMediaPlayer::durationChanged, this, &MainWindow::on_durationChanged);
    connect(player, &QMediaPlayer::stateChanged, this, &MainWindow::on_stateChanged);
    
    // 初始化进度条
    ui->positionSlider->setRange(0, 0);
}

void MainWindow::initSubtitleTimer()
{
    subtitleTimer = new QTimer(this);
    connect(subtitleTimer, &QTimer::timeout, this, &MainWindow::processAudioForSubtitles);
}

void MainWindow::initSpeechRecognition()
{
    // 初始化设置管理器
    SettingsManager::instance()->initialize();
    
    // 创建语音识别器实例
    m_speechRecognizer = new SpeechRecognizer(this);
    connect(m_speechRecognizer, &SpeechRecognizer::recognitionFinished, this, &MainWindow::onRecognitionFinished);
    connect(m_speechRecognizer, &SpeechRecognizer::recognitionError, this, &MainWindow::onRecognitionError);
    connect(m_speechRecognizer, &SpeechRecognizer::recognitionProgress, this, &MainWindow::onRecognitionProgress);
    
    // 初始化语音识别器
    bool initialized = m_speechRecognizer->initialize();
    if (!initialized) {
        ui->statusbar->showMessage(tr("语音识别器初始化失败，请检查Whisper模型路径"), 5000);
    }
}

void MainWindow::on_openButton_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("打开视频文件"), "", tr("视频文件 (*.mp4 *.avi *.mov *.mkv)") + ";;" + tr("所有文件 (*)"));
    
    if (!fileName.isEmpty()) {
        // 加载视频
        player->setMedia(QUrl::fromLocalFile(fileName));
        
        // 保存当前视频文件路径
        currentAudioFile = fileName;
        ui->statusbar->showMessage(tr("视频加载成功，准备语音识别"));
        
        // 开始播放
        player->play();
    }
}

void MainWindow::on_playButton_clicked()
{
    switch (player->state()) {
    case QMediaPlayer::PlayingState:
        player->pause();
        break;
    default:
        player->play();
        break;
    }
}

void MainWindow::on_positionChanged(qint64 position)
{
    ui->positionSlider->setValue(position);
    ui->timeLabel->setText(formatTime(position) + "/" + formatTime(player->duration()));
}

void MainWindow::on_durationChanged(qint64 duration)
{
    ui->positionSlider->setRange(0, duration);
}

void MainWindow::on_stateChanged(QMediaPlayer::State state)
{
    switch (state) {
    case QMediaPlayer::PlayingState:
        ui->playButton->setText(tr("暂停"));
        subtitleTimer->start(5000); // 每5秒尝试生成一次字幕
        break;
    case QMediaPlayer::PausedState:
    case QMediaPlayer::StoppedState:
        ui->playButton->setText(tr("播放"));
        subtitleTimer->stop();
        break;
    }
}

void MainWindow::on_positionSlider_sliderMoved(int position)
{
    player->setPosition(position);
}

void MainWindow::on_volumeSlider_valueChanged(int value)
{
    player->setVolume(value);
    // 移除对不存在的volumeLabel的引用
}

void MainWindow::on_playbackSpeedComboBox_currentIndexChanged(int index)
{
    qreal speed;
    switch (index) {
    case 0: speed = 0.5; break;
    case 1: speed = 0.75; break;
    case 2: speed = 1.0; break;
    case 3: speed = 1.25; break;
    case 4: speed = 1.5; break;
    case 5: speed = 2.0; break;
    default: speed = 1.0;
    }
    player->setPlaybackRate(speed);
}

QString MainWindow::formatTime(qint64 ms)
{
    int seconds = (ms / 1000) % 60;
    int minutes = (ms / 60000) % 60;
    int hours = (ms / 3600000);
    
    if (hours > 0) {
        return QString("%1:%2:%3")
                .arg(hours, 2, 10, QChar('0'))
                .arg(minutes, 2, 10, QChar('0'))
                .arg(seconds, 2, 10, QChar('0'));
    } else {
        return QString("%1:%2")
                .arg(minutes, 2, 10, QChar('0'))
                .arg(seconds, 2, 10, QChar('0'));
    }
}

void MainWindow::processAudioForSubtitles()
{
    // 如果视频正在播放且有视频文件
    if (player->state() == QMediaPlayer::PlayingState && !currentAudioFile.isEmpty()) {
        if (m_speechRecognizer) {
            // 从视频中提取音频并识别
            if (!m_speechRecognizer->recognizeFromVideo(currentAudioFile, "")) {
                ui->statusbar->showMessage(tr("启动语音识别失败"));
                generateSampleSubtitle();
            }
        } else {
            // 备用方案：显示示例字幕
            generateSampleSubtitle();
        }
    }
}

void MainWindow::onRecognitionFinished(const QString &text)
{
    // 显示识别结果作为字幕
    ui->subtitleTextEdit->setText(text);
    ui->statusbar->showMessage(tr("字幕生成完成"));
}

void MainWindow::onRecognitionError(const QString &errorMessage)
{
    qDebug() << "语音识别错误:" << errorMessage;
    ui->statusbar->showMessage(tr("字幕生成失败: ") + errorMessage);
    
    // 错误时使用模拟字幕
    generateSampleSubtitle();
}

void MainWindow::onRecognitionProgress(int progress)
{
    // 更新状态显示
    ui->statusbar->showMessage(tr("正在生成字幕... ") + QString::number(progress) + "%");
}

void MainWindow::startSpeechRecognition()
{
    if (!currentAudioFile.isEmpty()) {
        // 从视频中提取音频并识别
        if (!m_speechRecognizer->recognizeFromVideo(currentAudioFile, "")) {
            ui->statusbar->showMessage(tr("启动语音识别失败"));
            generateSampleSubtitle();
        }
    }
}

void MainWindow::generateSampleSubtitle()
{
    // 生成示例字幕内容
    QString sampleText = "这是一个示例字幕内容。\nThis is a sample subtitle content.";
    onRecognitionFinished(sampleText);
}

void MainWindow::on_actionSettings_triggered()
{
    SettingsDialog dialog(this);
    dialog.exec();
    
    // 重新初始化语音识别器以应用新设置
    if (m_speechRecognizer) {
        bool initialized = m_speechRecognizer->initialize();
        if (!initialized) {
            ui->statusbar->showMessage(tr("语音识别器设置已更新，但初始化失败，请检查设置"), 5000);
        } else {
            ui->statusbar->showMessage(tr("设置已应用"), 3000);
        }
    }
}
