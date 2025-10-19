#ifndef PLAYBACKWINDOW_H
#define PLAYBACKWINDOW_H

#include <QMainWindow>
#include <QMediaPlayer>
#include <QVideoWidget>

namespace Ui {
class PlaybackWindow;
}

class PlaybackWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit PlaybackWindow(QWidget *parent = nullptr);
    ~PlaybackWindow();
    
    // 设置要播放的文件路径
    void setMediaFilePath(const QString &filePath);
    
    // 设置字幕内容
    void setSubtitleContent(const QString &subtitle);

signals:
    // 当用户点击返回按钮时发出信号
    void backToRecognitionRequested();

private slots:
    // 播放控制槽函数
    void on_playButton_clicked();
    void on_pauseButton_clicked();
    void on_stopButton_clicked();
    void on_positionChanged(qint64 position);
    void on_durationChanged(qint64 duration);
    void on_stateChanged(QMediaPlayer::State state);
    
    // 返回语音识别界面
    void on_backToRecognitionButton_clicked();
    
    // 进度条拖动
    void on_positionSlider_sliderMoved(int position);

private:
    Ui::PlaybackWindow *ui;
    QMediaPlayer *player;
    QVideoWidget *videoWidget;
    QString currentMediaFile;
    QString currentSubtitle;
    
    // 格式化时间显示
    QString formatTime(qint64 milliseconds);
    
    // 记录日志
    void logMessage(const QString &message, const QString &level = "INFO");
};

#endif // PLAYBACKWINDOW_H