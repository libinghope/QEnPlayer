#include "speechrecognizer.h"
#include "settingsmanager.h"

#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QFile>
#include <QJsonParseError>
#include <QCoreApplication>
#include <cstring>
#include <fstream>
#include <vector>
#include <algorithm>

SpeechRecognizer::SpeechRecognizer(QObject *parent) : QObject(parent)
{
    m_whisperProcess = nullptr;
    m_networkManager = nullptr;
    m_whisperCtx = nullptr;
    m_recognitionThread = nullptr;
    
    // 初始化成员变量为默认值
    m_language = "auto";
    m_modelSize = "small";
    m_apiUrl = "https://api.example.com/asr";
    m_preferOnlineAPI = false;
    m_isRecognizing = false;
    m_audioSampleRate = 0;
    m_shouldStop = false;
    
    // 连接设置更改信号
    connect(SettingsManager::instance(), &SettingsManager::settingsChanged, this, &SpeechRecognizer::applySettings);
    
    // 初始化时进行FFmpeg可用性检查
    qCritical() << "[SpeechRecognizer] 初始化中，检查FFmpeg可用性...";
    bool ffmpegAvailable = isFfmpegAvailable();
    qCritical() << "[SpeechRecognizer] FFmpeg可用性检查结果:" << (ffmpegAvailable ? "可用" : "不可用");
    if (!ffmpegAvailable) {
        qCritical() << "[SpeechRecognizer] 警告: FFmpeg不可用，语音识别功能可能无法正常工作";
    }
    
    // 应用当前设置
    applySettings();
}

SpeechRecognizer::~SpeechRecognizer()
{
    qInfo() << "[SpeechRecognizer] 析构函数开始执行";
    
    // 首先调用cleanup清理大部分资源
    cleanup();
    
    // 在析构函数中安全地释放whisper上下文
    if (m_whisperCtx) {
        qInfo() << "[SpeechRecognizer] 释放Whisper上下文";
        whisper_free(m_whisperCtx);
        m_whisperCtx = nullptr;
    }
    
    // 确保网络管理器被释放
    if (m_networkManager) {
        delete m_networkManager;
        m_networkManager = nullptr;
    }
    
    qInfo() << "[SpeechRecognizer] 析构函数执行完成";
}

bool SpeechRecognizer::initialize(const QString &modelPath)
{
    qInfo() << "[SpeechRecognizer] 开始初始化...";
    
    // 首先检查FFmpeg是否可用
    bool ffmpegAvailable = isFfmpegAvailable();
    qInfo() << "[SpeechRecognizer] FFmpeg可用性检查结果:" << (ffmpegAvailable ? "可用" : "不可用");
    if (!ffmpegAvailable) {
        qWarning() << "[SpeechRecognizer] 警告: FFmpeg不可用，音频提取功能将无法工作。请安装FFmpeg并确保其在系统PATH中。";
    }
    
    // 清理之前的资源，但不释放whisper上下文
    if (m_recognitionThread) {
        qInfo() << "[SpeechRecognizer] 停止之前的识别线程";
        m_shouldStop = true;
        m_recognitionThread->wait(3000);
        delete m_recognitionThread;
        m_recognitionThread = nullptr;
    }
    
    // 清理其他资源
    m_currentAudioFile.clear();
    m_audioSamples.clear();
    m_isRecognizing = false;
    m_shouldStop = false;
    
    // 停止并清理Whisper进程
    if (m_whisperProcess && m_whisperProcess->state() == QProcess::Running) {
        m_whisperProcess->terminate();
        m_whisperProcess->waitForFinished(1000);
    }
    if (m_whisperProcess) {
        delete m_whisperProcess;
        m_whisperProcess = nullptr;
    }
    
    // 应用当前设置
    applySettings();
    
    // 设置模型路径
    if (!modelPath.isEmpty()) {
        m_whisperPath = modelPath;
        qDebug() << "使用提供的模型路径:" << m_whisperPath;
    } else {
        // 从设置管理器获取路径
        SettingsManager *settings = SettingsManager::instance();
        QString path = settings->getWhisperPath();
        if (!path.isEmpty()) {
            m_whisperPath = path;
            qDebug() << "使用设置中的模型路径:" << m_whisperPath;
        } else {
            // 默认使用whisper目录下的模型
            QString defaultModelPath = QCoreApplication::applicationDirPath() + "/../whisper/models/ggml-small.en.bin";
            if (QFile::exists(defaultModelPath)) {
                m_whisperPath = defaultModelPath;
                qDebug() << "使用默认模型路径:" << m_whisperPath;
            } else {
                // 尝试其他常见位置
                QStringList possiblePaths = {
                    QDir::homePath() + "/.local/share/whisper/ggml-small.en.bin",
                    "/usr/local/share/whisper/ggml-small.en.bin",
                    "/opt/homebrew/share/whisper/ggml-small.en.bin"
                };
                
                for (const QString &path : possiblePaths) {
                    qDebug() << "检查模型路径:" << path;
                    if (QFile::exists(path)) {
                        m_whisperPath = path;
                        qDebug() << "找到模型:" << m_whisperPath;
                        break;
                    }
                }
            }
        }
    }
    
    // 释放旧的whisper上下文（如果存在）
    if (m_whisperCtx) {
        qDebug() << "释放旧的Whisper上下文";
        whisper_free(m_whisperCtx);
        m_whisperCtx = nullptr;
    }
    
    // 如果有模型路径，尝试初始化whisper上下文
    if (!m_whisperPath.isEmpty() && QFile::exists(m_whisperPath)) {
        qDebug() << "从路径加载Whisper模型:" << m_whisperPath;
        
        // 检查模型文件扩展名
        QString extension = QFileInfo(m_whisperPath).suffix().toLower();
        if (extension == "pt") {
            qWarning() << "警告: 检测到PyTorch格式(.pt)的模型文件，这与whisper.cpp不兼容。";
            qWarning() << "请使用GGML格式的模型文件(.bin, .ggml, .ggmlv3)";
        }
        
        try {
            // 使用推荐的API并设置默认参数
            whisper_context_params ctx_params = whisper_context_default_params();
            
            // 尝试初始化模型并记录详细错误信息
            qDebug() << "尝试初始化Whisper上下文...";
            m_whisperCtx = whisper_init_from_file_with_params(m_whisperPath.toUtf8().constData(), ctx_params);
            
            if (m_whisperCtx == nullptr) {
                qWarning() << "初始化Whisper上下文失败:" << m_whisperPath;
                qWarning() << "可能的原因:";
                qWarning() << "1. 模型文件格式不兼容 (需要GGML格式而非PyTorch格式)";
                qWarning() << "2. 模型文件损坏";
                qWarning() << "3. 模型文件与当前whisper.cpp版本不兼容";
                qWarning() << "建议使用: models/ggml-small.en.bin 或其他GGML格式模型";
            } else {
                qDebug() << "Whisper上下文初始化成功";
            }
        } catch (const std::exception &e) {
            qCritical() << "加载Whisper模型时发生异常:" << e.what();
            m_whisperCtx = nullptr;
        }
    } else {
        qWarning() << "Whisper模型文件未找到:" << m_whisperPath;
        qWarning() << "请下载模型并设置正确路径";
        qWarning() << "您可以使用: whisper/download-ggml-model.sh small 下载模型";
    }
    
    // 检查FFmpeg是否可用
    if (!isFfmpegAvailable()) {
        qWarning() << "FFmpeg不可用，音频提取将失败！";
    }
    
    // 确保网络管理器已创建
    if (!m_networkManager) {
        m_networkManager = new QNetworkAccessManager(this);
        connect(m_networkManager, &QNetworkAccessManager::finished, this, &SpeechRecognizer::handleOnlineAPIReply);
    }
    
    // 检查本地Whisper是否可用
    bool available = isLocalWhisperAvailable();
    qDebug() << "SpeechRecognizer初始化完成，本地Whisper可用性:" << available;
    return available;
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
    
    qDebug() << "Configured SpeechRecognizer with whisper.cpp:";
    qDebug() << "- Language:" << m_language;
    qDebug() << "- Model size (for compatibility):" << m_modelSize;
    qDebug() << "- API URL:" << m_apiUrl;
}

void SpeechRecognizer::applySettings()
{
    SettingsManager *settings = SettingsManager::instance();
    
    // 应用Whisper模型路径设置
    QString newModelPath = settings->getWhisperPath();
    if (!newModelPath.isEmpty() && newModelPath != m_whisperPath) {
        m_whisperPath = newModelPath;
        
        // 如果模型路径改变，重新初始化whisper上下文
    if (m_whisperCtx) {
        whisper_free(m_whisperCtx);
        m_whisperCtx = nullptr;
    }
    
    if (QFile::exists(m_whisperPath)) {
        qDebug() << "Loading new Whisper model from:" << m_whisperPath;
        
        // 检查模型文件扩展名
        QString extension = QFileInfo(m_whisperPath).suffix().toLower();
        if (extension == "pt") {
            qWarning() << "警告: 检测到PyTorch格式(.pt)的模型文件，这与whisper.cpp不兼容。";
            qWarning() << "请使用GGML格式的模型文件(.bin, .ggml, .ggmlv3)";
        }
        
        // 使用推荐的API并设置默认参数
        whisper_context_params ctx_params = whisper_context_default_params();
        m_whisperCtx = whisper_init_from_file_with_params(m_whisperPath.toUtf8().constData(), ctx_params);
        
        if (m_whisperCtx == nullptr) {
            qWarning() << "Failed to initialize Whisper context with new model:" << m_whisperPath;
            qWarning() << "可能的原因:";
            qWarning() << "1. 模型文件格式不兼容 (需要GGML格式而非PyTorch格式)";
            qWarning() << "2. 模型文件损坏";
            qWarning() << "3. 模型文件与当前whisper.cpp版本不兼容";
            qWarning() << "建议使用: models/ggml-small.en.bin 或其他GGML格式模型";
        } else {
            qDebug() << "Whisper context reinitialized successfully with new model.";
        }
    }
    }
    
    // 应用语言设置
    m_language = settings->getRecognitionLanguage();
    if (m_language.isEmpty()) {
        m_language = "auto";
    }
    
    // 应用模型大小设置
    m_modelSize = settings->getWhisperModelSize();
    if (m_modelSize.isEmpty()) {
        m_modelSize = "small";
    }
    
    // 应用API设置
    m_apiUrl = settings->getApiUrl();
    
    // 应用优先使用API设置
    m_preferOnlineAPI = settings->isPreferOnlineAPI();
    
    qDebug() << "Applied settings:";
    qDebug() << "- Whisper model path:" << m_whisperPath;
    qDebug() << "- Language:" << m_language;
    qDebug() << "- Model size (for compatibility):" << m_modelSize;
    qDebug() << "- API URL:" << m_apiUrl;
    qDebug() << "- Prefer online API:" << m_preferOnlineAPI;
}

bool SpeechRecognizer::recognizeFile(const QString &audioFilePath)
{
    // 检查是否已经在识别中
    if (m_isRecognizing) {
        emit recognitionError("Already recognizing audio.");
        return false;
    }
    
    // 检查音频文件是否存在
    if (!QFile::exists(audioFilePath)) {
        emit recognitionError("Audio file not found: " + audioFilePath);
        return false;
    }
    
    // 保存当前处理的音频文件路径
    m_currentAudioFile = audioFilePath;
    
    // 根据优先设置选择识别方式
    if (m_preferOnlineAPI && !m_apiUrl.isEmpty()) {
        // 优先使用在线API
        return recognizeWithOnlineAPI(audioFilePath);
    } else if (isLocalWhisperAvailable()) {
        // 如果优先使用本地模型或API不可用，则使用本地Whisper
        return recognizeWithWhisper(audioFilePath);
    } else if (!m_apiUrl.isEmpty()) {
        // 如果本地Whisper不可用但API可用，则使用API
        return recognizeWithOnlineAPI(audioFilePath);
    } else {
        // 两者都不可用
        emit recognitionError("Neither local Whisper nor online API available.");
        return false;
    }
}

void SpeechRecognizer::recognizeAudioAsync()
{
    if (!m_whisperCtx || m_audioSamples.empty()) {
        QMetaObject::invokeMethod(this, [=]() {
            emit recognitionError("Whisper上下文未初始化或没有音频数据。");
        });
        return;
    }
    
    qCritical() << "[SpeechRecognizer] 开始在独立线程中进行识别...";
    
    // 设置whisper参数
    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    
    // 设置通用参数
    params.n_threads = std::min(8, (int)QThread::idealThreadCount());
    params.translate = false;
    params.print_realtime = false;
    params.print_progress = false;
    params.print_timestamps = false;
    params.print_special = false;
    params.token_timestamps = false;
    params.thold_pt = 0.01f;
    params.thold_ptsum = 0.01f;
    params.max_len = 0;
    params.split_on_word = true;
    params.max_tokens = 0;
    
    // 设置语言
    if (m_language != "auto") {
        params.language = m_language.toUtf8().constData();
        params.detect_language = false;
    } else {
        params.language = nullptr; // nullptr表示自动检测语言
        params.detect_language = true;
    }
    
    // 执行语音识别
    if (whisper_full(m_whisperCtx, params, m_audioSamples.data(), m_audioSamples.size()) != 0) {
        QMetaObject::invokeMethod(this, [=]() {
            emit recognitionError("Whisper处理音频失败。");
        });
        return;
    }
    
    // 收集识别结果
    std::string fullText;
    const int n_segments = whisper_full_n_segments(m_whisperCtx);
    
    qCritical() << "[SpeechRecognizer] 识别完成，共有" << n_segments << "个文本片段";
    
    for (int i = 0; i < n_segments; ++i) {
        const char *text = whisper_full_get_segment_text(m_whisperCtx, i);
        if (text) {
            fullText += text;
            if (i < n_segments - 1) {
                fullText += " ";
            }
        }
    }
    
    // 发送结果信号
    const QString result = QString::fromUtf8(fullText.c_str());
    QMetaObject::invokeMethod(this, [=]() {
        qCritical() << "[SpeechRecognizer] 识别完成，结果长度:" << result.length() << "字符";
        emit recognitionFinished(result);
        m_isRecognizing = false;
    });
}

bool SpeechRecognizer::recognizeFromVideo(const QString &videoFilePath, const QString &audioOutputPath)
{
    qCritical() << "[SpeechRecognizer] 开始从视频中识别语音:" << videoFilePath;
    
    if (videoFilePath.isEmpty()) {
        QString errorMsg = "视频文件路径为空";
        qCritical() << "[SpeechRecognizer] 错误:" << errorMsg;
        emit recognitionError(errorMsg);
        return false;
    }
    
    if (!QFile::exists(videoFilePath)) {
        QString errorMsg = "视频文件不存在: " + videoFilePath;
        qCritical() << "[SpeechRecognizer] 错误:" << errorMsg;
        emit recognitionError(errorMsg);
        return false;
    }
    
    // 检查FFmpeg可用性
    if (!isFfmpegAvailable()) {
        QString errorMsg = "FFmpeg不可用，无法执行语音识别";
        qCritical() << "[SpeechRecognizer] 错误:" << errorMsg;
        emit recognitionError(errorMsg);
        return false;
    }
    
    // 检查Whisper上下文
    if (!m_whisperCtx) {
        QString errorMsg = "Whisper上下文未初始化，请检查模型路径";
        qCritical() << "[SpeechRecognizer] 错误:" << errorMsg;
        emit recognitionError(errorMsg);
        return false;
    }
    
    QFileInfo videoInfo(videoFilePath);
    qCritical() << "[SpeechRecognizer] 视频文件信息: 大小=" << videoInfo.size()/1024 << "KB, 可读取=" << videoInfo.isReadable();
    
    // 提取音频
    QString audioPath = audioOutputPath;
    bool useTempFile = audioPath.isEmpty();
    
    if (useTempFile) {
        // 生成临时文件名
        audioPath = QDir::tempPath() + QDir::separator() + 
                   videoInfo.baseName() + "_temp.wav";
        m_tempAudioFile = audioPath;
        qCritical() << "[SpeechRecognizer] 使用临时音频文件路径: " << audioPath;
    } else {
        qCritical() << "[SpeechRecognizer] 使用指定的音频输出路径: " << audioPath;
    }
    
    qCritical() << "[SpeechRecognizer] 开始从视频提取音频";
    if (!extractAudioFromVideo(videoFilePath, audioPath)) {
        qCritical() << "[SpeechRecognizer] 音频提取失败，将执行清理操作";
        cleanup();
        return false;
    }
    
    qCritical() << "[SpeechRecognizer] 音频提取成功，开始进行语音识别";
    // 进行识别
    bool success = recognizeFile(audioPath);
    if (!success) {
        qCritical() << "[SpeechRecognizer] recognizeFile 调用失败";
    }
    return success;
}

void SpeechRecognizer::stop()
{
    // 停止识别线程
    if (m_isRecognizing) {
        qDebug() << "Stopping recognition...";
        m_shouldStop = true;
        
        if (m_recognitionThread) {
            m_recognitionThread->wait(3000);
        }
        
        m_isRecognizing = false;
    }
    
    // 停止Whisper进程（兼容旧代码）
    if (m_whisperProcess && m_whisperProcess->state() == QProcess::Running) {
        m_whisperProcess->terminate();
        m_whisperProcess->waitForFinished(2000);
        if (m_whisperProcess->state() == QProcess::Running) {
            m_whisperProcess->kill();
        }
    }
    
    // 清理资源
    cleanup();
}

bool SpeechRecognizer::loadAudioFile(const QString &audioFilePath, std::vector<float> &samples, int &sampleRate)
{
    qCritical() << "[SpeechRecognizer] 加载音频文件:" << audioFilePath;
    
    // 检查文件是否存在
    if (!QFile::exists(audioFilePath)) {
        qCritical() << "[SpeechRecognizer] 错误: 音频文件不存在:" << audioFilePath;
        return false;
    }
    
    QFileInfo audioInfo(audioFilePath);
    qCritical() << "[SpeechRecognizer] 音频文件信息: 大小=" << audioInfo.size()/1024 << "KB, 可读取=" << audioInfo.isReadable();
    
    // 检查文件大小，过小的文件可能是无效的
    if (audioInfo.size() < 1024) { // 小于1KB的音频文件可能有问题
        qCritical() << "[SpeechRecognizer] 警告: 音频文件可能无效，文件大小过小:" << audioInfo.size() << "字节";
    }
    
    // 使用ffmpeg提取PCM音频数据
    QProcess ffmpegProcess;
    QStringList ffmpegArgs;
    ffmpegArgs << "-hide_banner" << "-i" << audioFilePath
               << "-f" << "f32le"
               << "-acodec" << "pcm_f32le"
               << "-ar" << "16000"
               << "-ac" << "1"
               << "-filter:a" << "atempo=1.0"
               << "-";
    
    qCritical() << "[SpeechRecognizer] 执行ffmpeg命令加载音频: ffmpeg" << ffmpegArgs.join(" ");
    
    ffmpegProcess.start("ffmpeg", ffmpegArgs);
    
    if (!ffmpegProcess.waitForStarted(2000)) {
        qCritical() << "[SpeechRecognizer] 错误: 无法启动ffmpeg进程读取音频文件";
        return false;
    }
    
    // 读取输出数据
    QByteArray audioData;
    qCritical() << "[SpeechRecognizer] 开始读取音频数据...";
    
    // 设置一个合理的超时时间
    int totalWaitTime = 0;
    const int maxWaitTime = 30000; // 30秒
    
    while (totalWaitTime < maxWaitTime) {
        if (ffmpegProcess.waitForReadyRead(1000)) {
            QByteArray chunk = ffmpegProcess.readAll();
            audioData += chunk;
            qCritical() << "[SpeechRecognizer] 读取到" << chunk.size() << "字节，累计" << audioData.size() << "字节";
        }
        
        if (ffmpegProcess.state() != QProcess::Running) {
            break;
        }
        
        totalWaitTime += 1000;
    }
    
    // 检查是否超时
    if (totalWaitTime >= maxWaitTime) {
        qCritical() << "[SpeechRecognizer] 错误: 读取音频数据超时(" << maxWaitTime/1000 << "秒)";
        ffmpegProcess.kill();
        return false;
    }
    
    // 检查退出码
    int exitCode = ffmpegProcess.exitCode();
    QString errorOutput = ffmpegProcess.readAllStandardError();
    
    qCritical() << "[SpeechRecognizer] ffmpeg处理音频退出码:" << exitCode;
    
    if (exitCode != 0) {
        qCritical() << "[SpeechRecognizer] 错误: ffmpeg处理音频失败，退出码:" << exitCode;
        qCritical() << "[SpeechRecognizer] ffmpeg错误输出:" << errorOutput.left(200);
        return false;
    }
    
    // 检查是否读取到数据
    if (audioData.isEmpty()) {
        qCritical() << "[SpeechRecognizer] 错误: 未能从音频文件读取数据";
        return false;
    }
    
    // 将字节数据转换为浮点数样本
    const size_t numSamples = audioData.size() / sizeof(float);
    qCritical() << "[SpeechRecognizer] 音频数据大小:" << audioData.size() << "字节，样本数:" << numSamples;
    
    if (numSamples == 0) {
        qCritical() << "[SpeechRecognizer] 错误: 音频数据转换失败，无法获取有效的音频样本";
        return false;
    }
    
    samples.resize(numSamples);
    memcpy(samples.data(), audioData.constData(), numSamples * sizeof(float));
    
    sampleRate = 16000; // 我们强制将采样率设置为16000Hz
    
    qCritical() << "[SpeechRecognizer] 音频文件加载成功，样本数:" << numSamples << "，采样率:" << sampleRate << "Hz";
    return true;
}

void SpeechRecognizer::cleanup()
{
    qCritical() << "[SpeechRecognizer] cleanup() 开始执行";
    
    // 停止识别线程
    if (m_recognitionThread && m_recognitionThread->isRunning()) {
        qCritical() << "[SpeechRecognizer] 停止识别线程...";
        m_shouldStop = true;
        bool stopped = m_recognitionThread->wait(3000); // 等待3秒
        
        if (!stopped) {
            qCritical() << "[SpeechRecognizer] 警告：识别线程未能及时停止，强制结束...";
        }
        
        try {
            delete m_recognitionThread;
            m_recognitionThread = nullptr;
            qCritical() << "[SpeechRecognizer] 识别线程已清理";
        } catch (const std::exception &e) {
            qCritical() << "[SpeechRecognizer] 删除识别线程时发生异常:" << e.what();
        }
    }
    
    // 停止并清理Whisper进程（如果有）
    if (m_whisperProcess && m_whisperProcess->state() == QProcess::Running) {
        qCritical() << "[SpeechRecognizer] 停止Whisper进程...";
        m_whisperProcess->terminate();
        if (!m_whisperProcess->waitForFinished(1000)) {
            qCritical() << "[SpeechRecognizer] 警告：Whisper进程未能终止，强制终止...";
            m_whisperProcess->kill();
        }
    }
    
    if (m_whisperProcess) {
        try {
            delete m_whisperProcess;
            m_whisperProcess = nullptr;
            qCritical() << "[SpeechRecognizer] Whisper进程已清理";
        } catch (const std::exception &e) {
            qCritical() << "[SpeechRecognizer] 删除Whisper进程时发生异常:" << e.what();
        }
    }
    
    // 清理临时文件
    if (!m_currentAudioFile.isEmpty() && QFile::exists(m_currentAudioFile)) {
        qCritical() << "[SpeechRecognizer] 删除临时音频文件:" << m_currentAudioFile;
        if (!QFile::remove(m_currentAudioFile)) {
            qCritical() << "[SpeechRecognizer] 警告：无法删除临时文件:" << m_currentAudioFile;
        }
        m_currentAudioFile.clear();
    }
    
    // 删除临时音频文件
    if (!m_tempAudioFile.isEmpty() && QFile::exists(m_tempAudioFile)) {
        if (QFile::remove(m_tempAudioFile)) {
            qCritical() << "[SpeechRecognizer] 已删除临时音频文件:" << m_tempAudioFile;
        } else {
            qCritical() << "[SpeechRecognizer] 警告：无法删除临时音频文件:" << m_tempAudioFile;
        }
        m_tempAudioFile.clear();
    }
    
    // 清理音频样本数据
    m_audioSamples.clear();
    
    // 重置状态
    m_isRecognizing = false;
    m_shouldStop = false;
    
    // 重要：whisper上下文的释放已移至initialize或析构函数中，避免在多线程环境下出现问题
    qCritical() << "[SpeechRecognizer] cleanup中不释放whisper上下文，由initialize或析构函数处理";
    
    // 网络管理器的删除已移至析构函数，避免在多线程环境下出现问题
    
    qCritical() << "[SpeechRecognizer] cleanup() 执行完成";
}

bool SpeechRecognizer::isLocalWhisperAvailable() const
{
    return m_whisperCtx != nullptr;
}

void SpeechRecognizer::setPreferOnlineAPI(bool prefer)
{
    m_preferOnlineAPI = prefer;
}

bool SpeechRecognizer::isFfmpegAvailable()
{
    qCritical() << "[SpeechRecognizer] 开始检查ffmpeg可用性";
    
    // 尝试启动ffmpeg进程并检查其可用性
    QProcess ffmpeg;
    ffmpeg.start("ffmpeg", QStringList() << "-version");
    
    if (!ffmpeg.waitForStarted()) {
        qCritical() << "[SpeechRecognizer] 无法启动ffmpeg进程，请检查ffmpeg是否已安装并在系统PATH中";
        
        // 尝试获取系统PATH信息
        QProcess envProcess;
        envProcess.start("echo", QStringList() << "$PATH");
        envProcess.waitForFinished();
        QString path = envProcess.readAllStandardOutput();
        qCritical() << "[SpeechRecognizer] 当前系统PATH:" << path;
        
        return false;
    }
    
    if (!ffmpeg.waitForFinished(2000)) {
        qCritical() << "[SpeechRecognizer] ffmpeg进程未在指定时间内完成";
        ffmpeg.kill();
        return false;
    }
    
    // 检查退出码
    int exitCode = ffmpeg.exitCode();
    QString stdout = ffmpeg.readAllStandardOutput();
    QString stderr = ffmpeg.readAllStandardError();
    
    qCritical() << "[SpeechRecognizer] ffmpeg进程退出码:" << exitCode;
    qCritical() << "[SpeechRecognizer] ffmpeg版本信息前100字符:" << stdout.left(100);
    
    if (!stderr.isEmpty()) {
        qCritical() << "[SpeechRecognizer] ffmpeg标准错误输出:" << stderr.left(200);
    }
    
    bool containsVersion = stdout.contains("ffmpeg version");
    qCritical() << "[SpeechRecognizer] ffmpeg输出包含版本信息:" << containsVersion;
    
    // 检查退出码和输出
    bool result = (exitCode == 0 && containsVersion);
    qCritical() << "[SpeechRecognizer] ffmpeg可用性检查结果:" << result;
    
    return result;
}

void SpeechRecognizer::handleWhisperOutput()
{
    // 注意：此方法现在已不再使用，因为我们直接使用whisper.cpp的C API
    // 保留此方法以保持API兼容性
    qDebug() << "handleWhisperOutput called but not used with whisper.cpp";
}

void SpeechRecognizer::handleWhisperError()
{
    // 注意：此方法现在已不再使用，因为我们直接使用whisper.cpp的C API
    // 保留此方法以保持API兼容性
    qDebug() << "handleWhisperError called but not used with whisper.cpp";
}

void SpeechRecognizer::handleWhisperFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    // 注意：此方法现在已不再使用，因为我们直接使用whisper.cpp的C API
    // 保留此方法以保持API兼容性
    qDebug() << "handleWhisperFinished called but not used with whisper.cpp";
}

void SpeechRecognizer::handleOnlineAPIReply(QNetworkReply *reply)
{
    if (!reply) {
        qDebug() << "handleOnlineAPIReply: null reply received";
        emit recognitionError("无效的API响应");
        cleanup();
        return;
    }
    
    // 保存当前状态
    bool shouldCleanup = true;
    
    try {
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
                qDebug() << "尝试使用本地Whisper模型...";
                recognizeWithWhisper(m_currentAudioFile);
                shouldCleanup = false; // 本地模型处理时不要清理，避免资源冲突
            }
        }
    } catch (const std::exception &e) {
        qCritical() << "异常捕获: " << e.what();
        emit recognitionError("处理API响应时发生异常");
    }
    
    reply->deleteLater();
    if (shouldCleanup) {
        cleanup();
    }
}

bool SpeechRecognizer::recognizeWithWhisper(const QString &audioFilePath)
{
    qCritical() << "[SpeechRecognizer] recognizeWithWhisper: 开始使用Whisper模型进行识别";
    
    // 首先检查FFmpeg是否可用
    if (!isFfmpegAvailable()) {
        QString errorMsg = "FFmpeg不可用，无法提取音频。请安装FFmpeg并确保其在系统PATH中。";
        qCritical() << "[SpeechRecognizer]" << errorMsg;
        emit recognitionError(errorMsg);
        return false;
    }
    
    if (!isLocalWhisperAvailable()) {
        QString errorMsg = "Whisper模型不可用，请检查模型路径和初始化";
        qCritical() << "[SpeechRecognizer]" << errorMsg;
        emit recognitionError(errorMsg);
        return false;
    }
    
    qCritical() << "[SpeechRecognizer] Whisper上下文已初始化，准备加载音频文件";
    
    // 加载音频文件
    m_audioSamples.clear();
    if (!loadAudioFile(audioFilePath, m_audioSamples, m_audioSampleRate)) {
        QString errorMsg = "加载音频文件失败: " + audioFilePath;
        qCritical() << "[SpeechRecognizer]" << errorMsg;
        emit recognitionError(errorMsg);
        return false;
    }
    
    // 检查音频样本是否有效
    if (m_audioSamples.empty()) {
        QString errorMsg = "音频样本为空，无法进行识别";
        qCritical() << "[SpeechRecognizer]" << errorMsg;
        emit recognitionError(errorMsg);
        return false;
    }
    
    qCritical() << "[SpeechRecognizer] 音频样本加载完成，样本数:" << m_audioSamples.size() << "，准备开始识别";
    
    // 设置识别状态
    m_isRecognizing = true;
    m_shouldStop = false;
    
    // 创建并启动新的识别线程
    m_recognitionThread = QThread::create([this]() {
        qCritical() << "[SpeechRecognizer] 识别线程启动";
        this->recognizeAudioAsync();
    });
    
    connect(m_recognitionThread, &QThread::finished, m_recognitionThread, &QObject::deleteLater);
    
    qCritical() << "[SpeechRecognizer] 识别线程已创建并启动";
    m_recognitionThread->start();
    
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
    qCritical() << "[CRITICAL] 开始extractAudioFromVideo，视频路径:" << videoFilePath;
    
    // 检查视频文件是否存在
    QFileInfo videoFileInfo(videoFilePath);
    if (!videoFileInfo.exists()) {
        QString errorMsg = "视频文件不存在: " + videoFilePath;
        qCritical() << "[CRITICAL]" << errorMsg;
        emit recognitionError(errorMsg);
        return false;
    }
    
    if (!videoFileInfo.isReadable()) {
        QString errorMsg = "视频文件不可读: " + videoFilePath;
        qCritical() << "[CRITICAL]" << errorMsg;
        emit recognitionError(errorMsg);
        return false;
    }
    
    qCritical() << "[CRITICAL] 视频文件存在且可读，大小:" << videoFileInfo.size() / 1024 << "KB";
    
    // 检查ffmpeg是否可用
    bool ffmpegAvailable = isFfmpegAvailable();
    qCritical() << "[CRITICAL] FFmpeg可用性检查结果:" << (ffmpegAvailable ? "可用" : "不可用");
    
    if (!ffmpegAvailable) {
        QString errorMsg = "FFmpeg未安装或不可用，请确保FFmpeg已安装并在系统PATH中";
        qCritical() << "[CRITICAL]" << errorMsg;
        emit recognitionError(errorMsg);
        return false;
    }
    
    // 如果未提供输出路径，生成临时文件路径
    if (audioOutputPath.isEmpty()) {
        qCritical() << "[CRITICAL] 未提供输出路径，生成临时文件路径";
        QFileInfo videoInfo(videoFilePath);
        audioOutputPath = QDir::tempPath() + QDir::separator() + 
                         videoInfo.baseName() + "_temp_" + 
                         QString::number(QDateTime::currentMSecsSinceEpoch()) + ".wav";
    }
    
    qCritical() << "[CRITICAL] 输出音频路径:" << audioOutputPath;
    
    // 确保目标输出目录存在
    QFileInfo outputInfo(audioOutputPath);
    QDir outputDir = outputInfo.dir();
    qCritical() << "[CRITICAL] 输出目录路径:" << outputDir.absolutePath();
    
    if (!outputDir.exists()) {
        qCritical() << "[CRITICAL] 输出目录不存在，尝试创建:" << outputDir.absolutePath();
        if (!outputDir.mkpath(".")) {
            QString errorMsg = "无法创建输出目录: " + outputDir.path();
            qCritical() << "[CRITICAL]" << errorMsg;
            emit recognitionError(errorMsg);
            return false;
        }
        qCritical() << "[CRITICAL] 输出目录创建成功";
    } else {
        qCritical() << "[CRITICAL] 输出目录已存在";
    }
    
    // 检查输出目录是否可读（Qt 5.14.2中没有isWritable方法）
    if (!outputDir.isReadable()) {
        QString errorMsg = "输出目录不可访问: " + outputDir.path();
        qCritical() << "[CRITICAL]" << errorMsg;
        emit recognitionError(errorMsg);
        return false;
    }
    
    qCritical() << "[CRITICAL] 输出目录可访问";
    
    // 构建ffmpeg命令行参数 - 移除-hide_banner以获取完整输出用于调试
    QStringList args;
    // args << "-hide_banner" << "-i" << videoFilePath; // 移除hide_banner以获取更多调试信息
    args << "-i" << videoFilePath;
    args << "-vn" << "-acodec" << "pcm_s16le" << "-ar" << "16000" << "-ac" << "1";
    args << "-y" << audioOutputPath; // 添加-y参数自动覆盖文件
    
    qCritical() << "[CRITICAL] 执行ffmpeg命令: ffmpeg" << args.join(" ");
    
    // 执行ffmpeg命令
    QProcess ffmpegProcess;
    ffmpegProcess.setProcessChannelMode(QProcess::MergedChannels); // 合并标准输出和错误输出
    
    ffmpegProcess.start("ffmpeg", args);
    qCritical() << "[CRITICAL] FFmpeg进程启动命令已发出";
    // 后续通过waitForStarted来检查是否真正启动成功
    
    // 等待处理开始，设置超时
    const int startTimeout = 5000; // 5秒
    if (!ffmpegProcess.waitForStarted(startTimeout)) {
        QString errorMsg = "无法启动ffmpeg进程或启动超时(" + QString::number(startTimeout/1000) + "秒)";
        qCritical() << "[CRITICAL]" << errorMsg;
        qCritical() << "[CRITICAL] 进程错误:" << ffmpegProcess.error();
        qCritical() << "[CRITICAL] 进程状态:" << ffmpegProcess.state();
        emit recognitionError(errorMsg);
        return false;
    }
    
    qCritical() << "[CRITICAL] FFmpeg进程已启动，PID:" << ffmpegProcess.processId();
    
    emit recognitionProgress(0); // 开始提取音频
    
    // 读取输出并显示进度
    QString output;
    const int maxProcessTime = 60000; // 60秒最大处理时间
    const qint64 startTime = QDateTime::currentMSecsSinceEpoch();
    
    while (ffmpegProcess.state() == QProcess::Running) {
        // 检查是否超时
        if (QDateTime::currentMSecsSinceEpoch() - startTime > maxProcessTime) {
            QString errorMsg = "音频提取超时(" + QString::number(maxProcessTime/1000) + "秒)";
            qCritical() << "[CRITICAL]" << errorMsg;
            ffmpegProcess.kill();
            emit recognitionError(errorMsg);
            return false;
        }
        
        if (ffmpegProcess.waitForReadyRead(1000)) {
            QByteArray newOutput = ffmpegProcess.readAll();
            QString outputStr = QString::fromLocal8Bit(newOutput);
            output += outputStr;
            qCritical() << "[CRITICAL] FFmpeg输出片段:" << outputStr.left(200) << (outputStr.length() > 200 ? "...(截断)" : "");
            // 简单的进度更新
            emit recognitionProgress(25);
        }
    }
    
    // 读取剩余输出
    if (ffmpegProcess.bytesAvailable() > 0) {
        QByteArray remainingOutput = ffmpegProcess.readAll();
        QString remainingOutputStr = QString::fromLocal8Bit(remainingOutput);
        output += remainingOutputStr;
        qCritical() << "[CRITICAL] FFmpeg剩余输出:" << remainingOutputStr.left(200) << (remainingOutputStr.length() > 200 ? "...(截断)" : "");
    }
    
    // 检查退出码
    int exitCode = ffmpegProcess.exitCode();
    qCritical() << "[CRITICAL] ffmpeg命令执行完成，退出码:" << exitCode;
    qCritical() << "[CRITICAL] ffmpeg退出状态:" << ffmpegProcess.exitStatus();
    
    // 记录ffmpeg完整输出以供调试
    qCritical() << "[CRITICAL] ffmpeg完整输出:\n" << output.left(1000) << (output.length() > 1000 ? "...(截断)" : "");
    
    // 检查退出码
    if (exitCode != 0) {
        QString errorMsg = "音频提取失败，ffmpeg退出码:" + QString::number(exitCode);
        qCritical() << "[CRITICAL]" << errorMsg;
        emit recognitionError(errorMsg + "\n" + output.left(500));
        return false;
    }
    
    // 检查输出文件是否存在
    if (QFile::exists(audioOutputPath)) {
        QFileInfo resultFile(audioOutputPath);
        qCritical() << "[CRITICAL] 音频提取成功:" << audioOutputPath << "(大小:" << resultFile.size() / 1024 << "KB)";
        
        // 额外检查文件大小
        if (resultFile.size() < 1024) { // 小于1KB
            qCritical() << "[CRITICAL] 警告：提取的音频文件非常小，可能存在问题";
        }
        
        emit recognitionProgress(50); // 音频提取完成，准备开始识别
        
        // 直接在当前线程加载音频文件并执行识别
        std::vector<float> audioSamples;
        int sampleRate;
        
        qCritical() << "[CRITICAL] 开始加载提取的音频文件进行语音识别";
        if (!loadAudioFile(audioOutputPath, audioSamples, sampleRate)) {
            QString errorMsg = "无法加载提取的音频文件: " + audioOutputPath;
            qCritical() << "[CRITICAL]" << errorMsg;
            emit recognitionError(errorMsg);
            return false;
        }
        
        qCritical() << "[CRITICAL] 音频文件加载成功，样本数:" << audioSamples.size() << "，采样率:" << sampleRate;
        
        // 检查Whisper上下文是否初始化
        if (!m_whisperCtx) {
            QString errorMsg = "Whisper上下文未初始化，请检查模型路径";
            qCritical() << "[CRITICAL]" << errorMsg;
            emit recognitionError(errorMsg);
            return false;
        }
        
        // 设置whisper参数
        whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
        
        // 设置通用参数
        params.n_threads = std::min(8, (int)QThread::idealThreadCount());
        params.translate = false;
        params.print_realtime = false;
        params.print_progress = false;
        params.print_timestamps = false;
        params.print_special = false;
        params.token_timestamps = false;
        params.thold_pt = 0.01f;
        params.thold_ptsum = 0.01f;
        params.max_len = 0;
        params.split_on_word = true;
        params.max_tokens = 0;
        
        // 设置语言
        if (m_language != "auto") {
            params.language = m_language.toUtf8().constData();
            params.detect_language = false;
        } else {
            params.language = nullptr; // nullptr表示自动检测语言
            params.detect_language = true;
        }
        
        emit recognitionProgress(75); // 开始执行语音识别
        qCritical() << "[CRITICAL] 开始执行Whisper语音识别";
        
        // 执行语音识别
        if (whisper_full(m_whisperCtx, params, audioSamples.data(), audioSamples.size()) != 0) {
            QString errorMsg = "Whisper处理音频失败，请检查模型和音频质量";
            qCritical() << "[CRITICAL]" << errorMsg;
            emit recognitionError(errorMsg);
            return false;
        }
        
        qCritical() << "[CRITICAL] Whisper识别完成，开始收集结果";
        
        // 收集识别结果
        std::string fullText;
        for (int i = 0; i < whisper_full_n_segments(m_whisperCtx); ++i) {
            const char* text = whisper_full_get_segment_text(m_whisperCtx, i);
            fullText += text;
            fullText += "\n";
        }
        
        QString recognizedText = QString::fromUtf8(fullText.c_str());
        qCritical() << "[CRITICAL] 识别结果长度:" << recognizedText.length() << "字符";
        
        emit recognitionProgress(100); // 识别完成
        emit recognitionFinished(recognizedText);
        
        return true;
    } else {
        QString errorMsg = "音频提取失败，输出文件不存在: " + audioOutputPath;
        qCritical() << "[CRITICAL]" << errorMsg;
        emit recognitionError(errorMsg);
        return false;
    }
}