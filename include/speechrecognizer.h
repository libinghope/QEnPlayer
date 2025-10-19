#ifndef SPEECHRECOGNIZER_H
#define SPEECHRECOGNIZER_H

#include <QObject>
#include <QString>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>
#include "whisper.h"

/**
 * @brief 语音识别器类
 * 
 * 封装了语音识别功能，提供本地Whisper模型和在线API两种识别方式
 */
class SpeechRecognizer : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit SpeechRecognizer(QObject *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~SpeechRecognizer();
    
    /**
     * @brief 初始化语音识别器
     * @param whisperPath Whisper模型路径，如果为空则自动搜索
     * @return 是否初始化成功
     */
    bool initialize(const QString &whisperPath = QString());
    
    /**
     * @brief 配置识别参数
     * @param language 语言代码，如"zh"、"en"等，默认为"auto"
     * @param modelSize 模型大小，如"small"、"medium"等
     * @param apiUrl 在线API地址
     */
    void configure(const QString &language = "auto", 
                   const QString &modelSize = "small",
                   const QString &apiUrl = "https://api.example.com/asr");
    
    /**
     * @brief 应用设置管理器中的设置
     */
    void applySettings();
    
    /**
     * @brief 开始识别音频文件
     * @param audioFilePath 音频文件路径
     * @return 是否成功开始识别
     */
    bool recognizeFile(const QString &audioFilePath);
    
    /**
     * @brief 从视频文件中提取音频并进行识别
     * @param videoFilePath 视频文件路径
     * @param audioOutputPath 提取的音频保存路径，如果为空则使用临时文件
     * @return 是否成功开始识别
     */
    bool recognizeFromVideo(const QString &videoFilePath, const QString &audioOutputPath = QString());
    
    /**
     * @brief 停止当前的识别任务
     */
    void stop();
    
    /**
     * @brief 检查本地Whisper模型是否可用
     * @return 是否可用
     */
    bool isLocalWhisperAvailable() const;
    
    /**
     * @brief 检查FFmpeg是否可用
     * @return 是否可用
     */
    bool isFfmpegAvailable();
    
    /**
     * @brief 设置是否优先使用在线API
     * @param prefer 为true时优先使用在线API，否则优先使用本地模型
     */
    void setPreferOnlineAPI(bool prefer);

signals:
    /**
     * @brief 识别结果信号
     * @param text 识别出的文本
     */
    void recognitionFinished(const QString &text);
    
    /**
     * @brief 识别出错信号
     * @param errorMessage 错误信息
     */
    void recognitionError(const QString &errorMessage);
    
    /**
     * @brief 识别进度信号
     * @param progress 进度值(0-100)
     */
    void recognitionProgress(int progress);

private slots:
    /**
     * @brief 处理Whisper进程输出
     */
    void handleWhisperOutput();
    
    /**
     * @brief 处理Whisper进程错误
     */
    void handleWhisperError();
    
    /**
     * @brief 处理Whisper进程完成
     */
    void handleWhisperFinished(int exitCode, QProcess::ExitStatus exitStatus);
    
    /**
     * @brief 处理在线API响应
     */
    void handleOnlineAPIReply(QNetworkReply *reply);

private:
    /**
     * @brief 使用本地Whisper模型进行识别
     * @param audioFilePath 音频文件路径
     * @return 是否成功开始识别
     */
    bool recognizeWithWhisper(const QString &audioFilePath);
    
    /**
     * @brief 使用在线API进行识别
     * @param audioFilePath 音频文件路径
     * @return 是否成功开始识别
     */
    bool recognizeWithOnlineAPI(const QString &audioFilePath);
    
    /**
     * @brief 从视频中提取音频
     * @param videoFilePath 视频文件路径
     * @param audioOutputPath 提取的音频保存路径
     * @return 是否提取成功
     */
    bool extractAudioFromVideo(const QString &videoFilePath, QString &audioOutputPath);
    
    /**
     * @brief 清理临时资源
     */
    void cleanup();
    
    /**
     * @brief 加载音频文件到内存
     * @param audioFilePath 音频文件路径
     * @param samples 输出的音频样本
     * @param sampleRate 输出的采样率
     * @return 是否加载成功
     */
    bool loadAudioFile(const QString &audioFilePath, std::vector<float> &samples, int &sampleRate);
    
    /**
     * @brief 异步执行语音识别的方法
     */
    void recognizeAudioAsync();
    
    // 成员变量
    QProcess *m_whisperProcess;              ///< 旧的Whisper进程（用于兼容）
    QNetworkAccessManager *m_networkManager; ///< 网络访问管理器
    QString m_whisperPath;                   ///< Whisper模型文件路径
    QString m_language;                      ///< 识别语言
    QString m_modelSize;                     ///< 模型大小
    QString m_apiUrl;                        ///< 在线API地址
    bool m_preferOnlineAPI;                  ///< 是否优先使用在线API
    QString m_currentAudioFile;              ///< 当前处理的音频文件
    QString m_tempAudioFile;                 ///< 临时音频文件（如果使用）
    
    // whisper.cpp相关成员
    whisper_context *m_whisperCtx;           ///< Whisper上下文
    QThread *m_recognitionThread;            ///< 识别线程
    bool m_isRecognizing;                    ///< 是否正在识别
    std::vector<float> m_audioSamples;       ///< 音频样本数据
    int m_audioSampleRate;                   ///< 音频采样率
    bool m_shouldStop;                       ///< 是否应该停止识别
};

#endif // SPEECHRECOGNIZER_H