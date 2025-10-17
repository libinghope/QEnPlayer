#include "speechrecognizer.h"
#include "settingsmanager.h"

#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QFile>
#include <QJsonParseError>

SpeechRecognizer::SpeechRecognizer(QObject *parent) : QObject(parent)
{
    m_whisperProcess = nullptr;
    m_networkManager = nullptr;
    
    // 初始化成员变量为默认值
    m_language = "auto";
    m_modelSize = "small";
    m_apiUrl = "https://api.example.com/asr";
    m_preferOnlineAPI = false;
    
    // 连接设置更改信号
    connect(SettingsManager::instance(), &SettingsManager::settingsChanged, this, [=]() {
        // 当设置更改时，更新内部配置
        applySettings();
    });
    
    // 应用当前设置
    applySettings();
}

SpeechRecognizer::~SpeechRecognizer()
{
    cleanup();
}

bool SpeechRecognizer::initialize(const QString &whisperPath)
{
    // 清理之前的资源
    cleanup();
    
    // 应用当前设置
    applySettings();
    
    // 如果提供了路径，则优先使用
    if (!whisperPath.isEmpty()) {
        m_whisperPath = whisperPath;
    } else {
        // 从设置管理器获取路径
        SettingsManager *settings = SettingsManager::instance();
        QString path = settings->getWhisperPath();
        if (!path.isEmpty()) {
            m_whisperPath = path;
        } else {
            // 默认搜索路径
        QStringList possiblePaths = {
            "/usr/local/bin/whisper",
            "/opt/homebrew/bin/whisper",
            QDir::homePath() + "/.local/bin/whisper"
        };
        
        for (const QString &path : possiblePaths) {
            if (QFile::exists(path)) {
                m_whisperPath = path;
                break;
            }
        }
        }
    }
    
    // 初始化网络管理器
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &SpeechRecognizer::handleOnlineAPIReply);
    
    // 初始化Whisper进程
    m_whisperProcess = new QProcess(this);
    connect(m_whisperProcess, &QProcess::readyReadStandardOutput, this, &SpeechRecognizer::handleWhisperOutput);
    connect(m_whisperProcess, &QProcess::readyReadStandardError, this, &SpeechRecognizer::handleWhisperError);
    connect(m_whisperProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), 
            this, &SpeechRecognizer::handleWhisperFinished);
    
    return true;
}

void SpeechRecognizer::configure(const QString &language, const QString &modelSize, const QString &apiUrl)
{
    if (!language.isEmpty()) {
        m_language = language;
    }
    
    if (!modelSize.isEmpty()) {
        m_modelSize = modelSize;
    }
    
    if (!apiUrl.isEmpty()) {
        m_apiUrl = apiUrl;
    }
}

void SpeechRecognizer::applySettings()
{
    // 从设置管理器获取配置
    SettingsManager *settings = SettingsManager::instance();
    
    m_language = settings->getRecognitionLanguage();
    m_modelSize = settings->getWhisperModelSize();
    m_apiUrl = settings->getApiUrl();
    m_preferOnlineAPI = settings->isPreferOnlineAPI();
    m_whisperPath = settings->getWhisperPath();
    
    qDebug() << "Settings applied to SpeechRecognizer:";
    qDebug() << "  Language:" << m_language;
    qDebug() << "  Model Size:" << m_modelSize;
    qDebug() << "  Prefer Online API:" << m_preferOnlineAPI;
    qDebug() << "  Whisper Path:" << m_whisperPath;
}

bool SpeechRecognizer::recognizeFile(const QString &audioFilePath)
{
    if (audioFilePath.isEmpty() || !QFile::exists(audioFilePath)) {
        emit recognitionError("音频文件不存在或路径为空");
        return false;
    }
    
    m_currentAudioFile = audioFilePath;
    
    // 决定使用哪种识别方式
    if (m_preferOnlineAPI) {
        // 优先使用在线API
        if (!recognizeWithOnlineAPI(audioFilePath)) {
            // 如果在线API失败，尝试本地模型
            if (isLocalWhisperAvailable()) {
                return recognizeWithWhisper(audioFilePath);
            }
            return false;
        }
    } else {
        // 优先使用本地模型
        if (isLocalWhisperAvailable() && recognizeWithWhisper(audioFilePath)) {
            return true;
        } else {
            // 如果本地模型不可用或失败，尝试在线API
            return recognizeWithOnlineAPI(audioFilePath);
        }
    }
    
    return true;
}

bool SpeechRecognizer::recognizeFromVideo(const QString &videoFilePath, const QString &audioOutputPath)
{
    if (videoFilePath.isEmpty() || !QFile::exists(videoFilePath)) {
        emit recognitionError("视频文件不存在或路径为空");
        return false;
    }
    
    // 提取音频
    QString audioPath = audioOutputPath;
    bool useTempFile = audioPath.isEmpty();
    
    if (useTempFile) {
        // 生成临时文件名
        QFileInfo videoInfo(videoFilePath);
        audioPath = QDir::tempPath() + QDir::separator() + 
                   videoInfo.baseName() + "_temp.wav";
        m_tempAudioFile = audioPath;
    }
    
    if (!extractAudioFromVideo(videoFilePath, audioPath)) {
        emit recognitionError("音频提取失败");
        cleanup();
        return false;
    }
    
    // 进行识别
    return recognizeFile(audioPath);
}

void SpeechRecognizer::stop()
{
    cleanup();
}

void SpeechRecognizer::cleanup()
{
    // 停止Whisper进程
    if (m_whisperProcess && m_whisperProcess->state() == QProcess::Running) {
        m_whisperProcess->terminate();
        m_whisperProcess->waitForFinished(1000);
    }
    
    // 删除临时音频文件
    if (!m_tempAudioFile.isEmpty() && QFile::exists(m_tempAudioFile)) {
        QFile::remove(m_tempAudioFile);
        m_tempAudioFile.clear();
    }
    
    // 重置当前音频文件路径
    m_currentAudioFile.clear();
}

bool SpeechRecognizer::isLocalWhisperAvailable() const
{
    return !m_whisperPath.isEmpty() && QFile::exists(m_whisperPath);
}

void SpeechRecognizer::setPreferOnlineAPI(bool prefer)
{
    m_preferOnlineAPI = prefer;
}

bool SpeechRecognizer::isFfmpegAvailable()
{
    // 尝试启动ffmpeg进程并检查其可用性
    QProcess ffmpeg;
    ffmpeg.start("ffmpeg", QStringList() << "-version");
    
    if (!ffmpeg.waitForStarted()) {
        return false;
    }
    
    if (!ffmpeg.waitForFinished(2000)) {
        ffmpeg.kill();
        return false;
    }
    
    // 检查退出码和输出
    return ffmpeg.exitCode() == 0 && ffmpeg.readAllStandardOutput().contains("ffmpeg version");
}

void SpeechRecognizer::handleWhisperOutput()
{
    // 这里可以处理Whisper的实时输出，例如解析进度信息
    QByteArray output = m_whisperProcess->readAllStandardOutput();
    qDebug() << "Whisper output:" << output;
    
    // 简单的进度模拟，实际项目中可以解析Whisper的输出获取真实进度
    static int progress = 0;
    progress += 10;
    if (progress > 100) progress = 90; // 留10%给最后的结果处理
    emit recognitionProgress(progress);
}

void SpeechRecognizer::handleWhisperError()
{
    QByteArray errorOutput = m_whisperProcess->readAllStandardError();
    qDebug() << "Whisper error:" << errorOutput;
    
    // 这里可以根据错误信息做进一步处理
}

void SpeechRecognizer::handleWhisperFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        // 重新读取所有输出，尝试解析JSON结果
        QByteArray allOutput = m_whisperProcess->readAllStandardOutput();
        QJsonParseError error;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(allOutput, &error);
        
        if (error.error == QJsonParseError::NoError && jsonDoc.isObject()) {
            QJsonObject jsonObj = jsonDoc.object();
            if (jsonObj.contains("text")) {
                QString recognizedText = jsonObj["text"].toString();
                emit recognitionProgress(100);
                emit recognitionFinished(recognizedText);
                return;
            }
        }
        
        // 如果没有找到JSON格式的文本，尝试直接使用输出
        QString text = QString::fromUtf8(allOutput).trimmed();
        if (!text.isEmpty()) {
            emit recognitionProgress(100);
            emit recognitionFinished(text);
            return;
        }
    }
    
    // 如果本地识别失败，尝试在线API
    if (!m_preferOnlineAPI && !m_currentAudioFile.isEmpty()) {
        if (!recognizeWithOnlineAPI(m_currentAudioFile)) {
            emit recognitionError("本地模型识别失败，且在线API也不可用");
        }
    } else {
        emit recognitionError("本地模型识别失败");
    }
    
    cleanup();
}

void SpeechRecognizer::handleOnlineAPIReply(QNetworkReply *reply)
{
    if (!reply) {
        emit recognitionError("无效的API响应");
        cleanup();
        return;
    }
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        QJsonParseError error;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(response, &error);
        
        if (error.error == QJsonParseError::NoError && jsonDoc.isObject()) {
            QJsonObject jsonObj = jsonDoc.object();
            
            // 尝试不同的响应格式
            if (jsonObj.contains("text")) {
                QString recognizedText = jsonObj["text"].toString();
                emit recognitionProgress(100);
                emit recognitionFinished(recognizedText);
            } else if (jsonObj.contains("result")) {
                // 检查result是否为数组
                if (jsonObj["result"].isArray()) {
                    QJsonArray resultArray = jsonObj["result"].toArray();
                    QStringList results;
                    foreach (const QJsonValue &value, resultArray) {
                        results << value.toString();
                    }
                    QString recognizedText = results.join("");
                    emit recognitionProgress(100);
                    emit recognitionFinished(recognizedText);
                } else if (jsonObj["result"].isString()) {
                    // 如果result是字符串
                    QString recognizedText = jsonObj["result"].toString();
                    emit recognitionProgress(100);
                    emit recognitionFinished(recognizedText);
                } else {
                    emit recognitionError("无法解析API响应格式");
                }
            } else {
                emit recognitionError("API响应中未找到识别结果");
            }
        } else {
            emit recognitionError("解析API响应失败: " + error.errorString());
        }
    } else {
        QString errorMsg = "API请求失败: " + reply->errorString();
        qDebug() << errorMsg;
        emit recognitionError(errorMsg);
        
        // 如果在线API失败且本地模型可用，尝试本地模型
        if (m_preferOnlineAPI && isLocalWhisperAvailable() && !m_currentAudioFile.isEmpty()) {
            recognizeWithWhisper(m_currentAudioFile);
            reply->deleteLater();
            return;
        }
    }
    
    reply->deleteLater();
    cleanup();
}

bool SpeechRecognizer::recognizeWithWhisper(const QString &audioFilePath)
{
    if (!isLocalWhisperAvailable() || !QFile::exists(audioFilePath)) {
        return false;
    }
    
    // 确保之前的进程已经停止
    if (m_whisperProcess && m_whisperProcess->state() == QProcess::Running) {
        m_whisperProcess->terminate();
        m_whisperProcess->waitForFinished(1000);
    }
    
    // 构建命令行参数
    QStringList arguments;
    arguments << audioFilePath
              << "--model" << m_modelSize
              << "--language" << m_language
              << "--output-format" << "json";
    
    // 启动Whisper进程
    m_whisperProcess->start(m_whisperPath, arguments);
    
    if (!m_whisperProcess->waitForStarted(2000)) {
        qDebug() << "无法启动Whisper进程";
        return false;
    }
    
    emit recognitionProgress(5); // 初始进度
    return true;
}

bool SpeechRecognizer::recognizeWithOnlineAPI(const QString &audioFilePath)
{
    if (!m_networkManager || audioFilePath.isEmpty() || !QFile::exists(audioFilePath)) {
        return false;
    }
    
    // 准备在线API请求
    QUrl apiUrl(m_apiUrl);
    QNetworkRequest request(apiUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    // 准备请求数据
    QJsonObject requestData;
    requestData["audio_file"] = audioFilePath;
    requestData["language"] = m_language;
    requestData["model"] = "whisper-" + m_modelSize;
    
    QJsonDocument doc(requestData);
    QByteArray data = doc.toJson();
    
    // 发送请求
    m_networkManager->post(request, data);
    emit recognitionProgress(10); // API请求已发送
    return true;
}

bool SpeechRecognizer::extractAudioFromVideo(const QString &videoFilePath, QString &audioOutputPath)
{
    // 检查ffmpeg是否可用
    if (!isFfmpegAvailable()) {
        emit recognitionError("FFmpeg未安装，请先安装FFmpeg");
        return false;
    }
    
    // 如果未提供输出路径，生成临时文件路径
    if (audioOutputPath.isEmpty()) {
        QFileInfo videoInfo(videoFilePath);
        audioOutputPath = QDir::tempPath() + QDir::separator() + 
                         videoInfo.baseName() + "_temp.wav";
    }
    
    // 构建ffmpeg命令行参数
    QStringList args;
    args << "-i" << videoFilePath;
    args << "-vn" << "-acodec" << "pcm_s16le" << "-ar" << "16000" << "-ac" << "1";
    args << audioOutputPath;
    
    // 执行ffmpeg命令
    QProcess ffmpegProcess;
    ffmpegProcess.start("ffmpeg", args);
    
    // 等待处理完成
    if (!ffmpegProcess.waitForStarted()) {
        qDebug() << "Failed to start ffmpeg";
        return false;
    }
    
    emit recognitionProgress(0); // 开始提取音频
    
    // 使用较长的超时时间，因为视频可能很大
    if (!ffmpegProcess.waitForFinished(30000)) { // 30秒超时
        qDebug() << "ffmpeg process timed out";
        ffmpegProcess.kill();
        return false;
    }
    
    // 检查输出文件是否存在
    if (QFile::exists(audioOutputPath)) {
        qDebug() << "Audio extracted successfully:" << audioOutputPath;
        return true;
    } else {
        qDebug() << "Failed to extract audio, output file not found";
        return false;
    }
}