#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QFileDialog>
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
    void on_playButton_clicked();
    void on_positionSlider_sliderMoved(int position);
    
    // 自定义槽函数
    void processAudioForSubtitles();
    
    // 语音识别相关槽函数
    void startSpeechRecognition();
    void onRecognitionFinished(const QString &text);
    void onRecognitionError(const QString &errorMessage);
    void onRecognitionProgress(int progress);
    
    // 设置相关槽函数
    void on_actionSettings_triggered();
    
    // 媒体播放器相关槽函数
    void on_positionChanged(qint64 position);
    void on_durationChanged(qint64 duration);
    void on_stateChanged(QMediaPlayer::State state);
    void on_volumeSlider_valueChanged(int value);
    void on_playbackSpeedComboBox_currentIndexChanged(int index);
    void generateSampleSubtitle();
    
private:
    Ui::MainWindow *ui;
    QMediaPlayer *player;
    QVideoWidget *videoWidget;
    QTimer *subtitleTimer;
    SpeechRecognizer *m_speechRecognizer; // 语音识别器
    QString currentAudioFile;      // 当前处理的视频文件路径
    
    void initPlayer();
    void initSubtitleTimer();
    void initSpeechRecognition();
    QString formatTime(qint64 milliseconds);
};
#endif // MAINWINDOW_H
