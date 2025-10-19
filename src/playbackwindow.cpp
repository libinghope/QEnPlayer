#include "playbackwindow.h"
#include <QVideoWidget>
#include <QDateTime>
#include <QDebug>
#include <QMessageBox>
#include "../forms/ui_playbackwindow.h"

PlaybackWindow::PlaybackWindow(QWidget *parent) : QMainWindow(parent),
                                                  ui(new Ui::PlaybackWindow),
                                                  player(nullptr),
                                                  videoWidget(nullptr)
{
    ui->setupUi(this);

    // 初始化播放器
    player = new QMediaPlayer(this);
    videoWidget = new QVideoWidget(ui->videoWidget);
    videoWidget->setStyleSheet("background-color: black;");

    QVBoxLayout *videoLayout = new QVBoxLayout(ui->videoWidget);
    videoLayout->addWidget(videoWidget);
    videoLayout->setContentsMargins(0, 0, 0, 0);

    player->setVideoOutput(videoWidget);

    // 连接信号槽
    connect(player, &QMediaPlayer::positionChanged, this, &PlaybackWindow::on_positionChanged);
    connect(player, &QMediaPlayer::durationChanged, this, &PlaybackWindow::on_durationChanged);
    connect(player, &QMediaPlayer::stateChanged, this, &PlaybackWindow::on_stateChanged);
    connect(player, QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error), [this](QMediaPlayer::Error error)
            { logMessage(QString("媒体播放错误: %1").arg(player->errorString()), "ERROR"); });

    // 初始禁用播放控制按钮
    ui->playButton->setEnabled(false);
    ui->pauseButton->setEnabled(false);
    ui->stopButton->setEnabled(false);

    logMessage("音频播放器已初始化", "INFO");
}

PlaybackWindow::~PlaybackWindow()
{
    delete videoWidget;
    delete player;
    delete ui;
}

void PlaybackWindow::setMediaFilePath(const QString &filePath)
{
    currentMediaFile = filePath;
    player->setMedia(QUrl::fromLocalFile(filePath));
    ui->statusbar->showMessage(QString("已加载文件: %1").arg(filePath), 3000);
    logMessage(QString("已设置媒体文件路径: %1").arg(filePath), "INFO");

    // 启用播放控制按钮
    ui->playButton->setEnabled(true);
}

void PlaybackWindow::setSubtitleContent(const QString &subtitle)
{
    currentSubtitle = subtitle;
    ui->subtitleTextEdit->setText(subtitle);
    logMessage("字幕内容已更新", "INFO");
}

void PlaybackWindow::on_playButton_clicked()
{
    player->play();
    logMessage("开始播放", "INFO");
}

void PlaybackWindow::on_pauseButton_clicked()
{
    player->pause();
    logMessage("暂停播放", "INFO");
}

void PlaybackWindow::on_stopButton_clicked()
{
    player->stop();
    logMessage("停止播放", "INFO");
}

void PlaybackWindow::on_positionChanged(qint64 position)
{
    ui->positionSlider->setValue(position);
    ui->timeLabel->setText(QString("%1 / %2").arg(formatTime(position)).arg(formatTime(player->duration())));
}

void PlaybackWindow::on_durationChanged(qint64 duration)
{
    ui->positionSlider->setRange(0, duration);
    ui->timeLabel->setText(QString("%1 / %2").arg(formatTime(player->position())).arg(formatTime(duration)));
}

void PlaybackWindow::on_stateChanged(QMediaPlayer::State state)
{
    switch (state)
    {
    case QMediaPlayer::PlayingState:
        ui->playButton->setEnabled(false);
        ui->pauseButton->setEnabled(true);
        ui->stopButton->setEnabled(true);
        ui->statusbar->showMessage("播放中", 2000);
        break;
    case QMediaPlayer::PausedState:
        ui->playButton->setEnabled(true);
        ui->pauseButton->setEnabled(false);
        ui->stopButton->setEnabled(true);
        ui->statusbar->showMessage("已暂停", 2000);
        break;
    case QMediaPlayer::StoppedState:
        ui->playButton->setEnabled(true);
        ui->pauseButton->setEnabled(false);
        ui->stopButton->setEnabled(false);
        ui->statusbar->showMessage("已停止", 2000);
        break;
    default:
        break;
    }
}

void PlaybackWindow::on_backToRecognitionButton_clicked()
{
    logMessage("用户请求返回语音识别界面", "INFO");
    emit backToRecognitionRequested();
}

void PlaybackWindow::on_positionSlider_sliderMoved(int position)
{
    player->setPosition(position);
    logMessage(QString("进度调整至: %1").arg(formatTime(position)), "INFO");
}

QString PlaybackWindow::formatTime(qint64 milliseconds)
{
    qint64 seconds = milliseconds / 1000;
    qint64 minutes = seconds / 60;
    qint64 hours = minutes / 60;

    seconds %= 60;
    minutes %= 60;

    if (hours > 0)
    {
        return QString("%1:%2:%3").arg(hours, 2, 10, QChar('0')).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
    }
    else
    {
        return QString("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
    }
}

void PlaybackWindow::logMessage(const QString &message, const QString &level)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString logEntry = QString("[%1] [%2] %3\n").arg(timestamp).arg(level).arg(message);

    ui->logTextEdit->append(logEntry);
    qDebug() << "[PlaybackWindow]" << message;
}