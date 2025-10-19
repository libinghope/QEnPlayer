#include <QCoreApplication>
#include <QDebug>
#include <QProcess>
#include <QFile>
#include <QTimer>
#include <QFileInfo>
#include <QString>
#include <QTextCodec>

// 简单的测试类来验证FFmpeg和语音识别功能
class SpeechRecognitionTester : public QObject {
    Q_OBJECT
public:
    SpeechRecognitionTester(QObject *parent = nullptr) : QObject(parent) {}

    void startTest() {
        qCritical() << "===== 语音识别功能测试开始 ====";
        
        // 1. 测试FFmpeg可用性
        testFfmpegAvailability();
        
        // 2. 测试音频提取功能
        testAudioExtraction();
        
        // 3. 完成测试
        qCritical() << "===== 语音识别功能测试结束 ====";
        QTimer::singleShot(1000, QCoreApplication::instance(), &QCoreApplication::quit);
    }

private:
    void testFfmpegAvailability() {
        qCritical() << "[测试] 检查FFmpeg可用性...";
        
        QProcess ffmpeg;
        ffmpeg.start("ffmpeg", QStringList() << "-version");
        
        if (!ffmpeg.waitForStarted(2000)) {
            qCritical() << "[错误] 无法启动ffmpeg进程";
            qCritical() << "[错误] 当前系统PATH:" << qgetenv("PATH");
            return;
        }
        
        if (!ffmpeg.waitForFinished(5000)) {
            qCritical() << "[错误] ffmpeg进程超时";
            ffmpeg.kill();
            return;
        }
        
        int exitCode = ffmpeg.exitCode();
        QString output = ffmpeg.readAllStandardOutput() + ffmpeg.readAllStandardError();
        
        qCritical() << "[测试] ffmpeg退出码:" << exitCode;
        qCritical() << "[测试] ffmpeg版本信息:" << output.left(100);
        
        if (exitCode == 0 && output.contains("ffmpeg version")) {
            qCritical() << "[成功] FFmpeg可用";
        } else {
            qCritical() << "[失败] FFmpeg不可用或版本不兼容";
        }
    }

    void testAudioExtraction() {
        qCritical() << "[测试] 测试音频提取功能...";
        
        // 检查测试视频是否存在
        QString testVideoPath = "../test_files/test_video_with_audio.mp4";
        QString outputAudioPath = "../test_files/extracted_audio.wav";
        
        if (!QFile::exists(testVideoPath)) {
            qCritical() << "[错误] 测试视频不存在:" << testVideoPath;
            return;
        }
        
        // 构建FFmpeg命令
        QStringList args;
        args << "-i" << testVideoPath
             << "-vn" << "-acodec" << "pcm_s16le" << "-ar" << "16000" << "-ac" << "1"
             << "-y" << outputAudioPath;
        
        qCritical() << "[测试] 执行FFmpeg命令: ffmpeg" << args.join(" ");
        
        QProcess ffmpeg;
        ffmpeg.start("ffmpeg", args);
        
        if (!ffmpeg.waitForStarted(2000)) {
            qCritical() << "[错误] 无法启动ffmpeg进程进行音频提取";
            return;
        }
        
        qCritical() << "[测试] 音频提取中，请稍候...";
        if (!ffmpeg.waitForFinished(10000)) {
            qCritical() << "[错误] 音频提取超时";
            ffmpeg.kill();
            return;
        }
        
        int exitCode = ffmpeg.exitCode();
        QString stdout = ffmpeg.readAllStandardOutput();
        QString stderr = ffmpeg.readAllStandardError();
        
        qCritical() << "[测试] 音频提取退出码:" << exitCode;
        
        if (!stdout.isEmpty()) {
            qCritical() << "[测试] FFmpeg标准输出:" << stdout.left(200);
        }
        
        if (!stderr.isEmpty()) {
            qCritical() << "[测试] FFmpeg标准错误:" << stderr.left(200);
        }
        
        if (exitCode == 0 && QFile::exists(outputAudioPath)) {
            QFileInfo fileInfo(outputAudioPath);
            qCritical() << "[成功] 音频提取完成，文件大小:" << fileInfo.size() << "字节";
        } else {
            qCritical() << "[失败] 音频提取失败";
        }
    }
};

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    // 设置UTF-8编码
    QTextCodec *codec = QTextCodec::codecForName("UTF-8");
    QTextCodec::setCodecForLocale(codec);
    
    SpeechRecognitionTester tester;
    QTimer::singleShot(100, &tester, &SpeechRecognitionTester::startTest);
    
    return app.exec();
}

#include "test_recognizer.moc"