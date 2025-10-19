#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QString>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "speechrecognizer.h"
#include "settingsmanager.h"
#include "settingsdialog.h"
#include "playbackwindow.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_openButton_clicked();
    void on_startRecognitionButton_clicked();
    
    // 自定义槽函数
    void processAudioForSubtitles();
    
    // 语音识别相关槽函数
    void startSpeechRecognition();
    void onRecognitionFinished(const QString &text);
    void onRecognitionError(const QString &errorMessage);
    void onRecognitionProgress(int progress);
    
    // 设置相关槽函数
    void on_actionSettings_triggered();
    void onSettingsChanged(); // 新增：处理设置变更的槽函数
    
    // 日志相关槽函数
    void on_clearLogButton_clicked();
    
    // 跳转到播放界面
    void on_goToPlaybackButton_clicked();
    
    // 从播放界面返回
    void onPlaybackWindowClosed();

private:
    Ui::MainWindow *ui;
    QTimer *subtitleTimer;
    SpeechRecognizer *m_speechRecognizer; // 语音识别器
    QString currentAudioFile;             // 当前处理的视频文件路径
    QString currentSubtitle;              // 当前识别的字幕内容
    bool isRecognitionInProgress;         // 标记识别任务是否正在进行中
    PlaybackWindow *playbackWindow;       // 播放窗口指针
    
    void initSubtitleTimer();
    void initSpeechRecognition();
    
    // FFmpeg可用性检查方法
    void checkFfmpegAvailability();
    
    // 显示识别完成提示并引导用户跳转
    void showRecognitionCompletePrompt();
    
public:
    // 日志相关方法 - 允许外部访问日志功能
    void logMessage(const QString &message, const QString &level = "INFO");
};
#endif // MAINWINDOW_H
