#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QObject>
#include <QSettings>
#include <QString>

/**
 * @brief 设置管理器类
 * 
 * 负责管理应用程序的所有配置设置，包括Whisper模型设置、字幕保存目录等
 */
class SettingsManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     * @return SettingsManager实例
     */
    static SettingsManager* instance();
    
    /**
     * @brief 初始化设置管理器
     * @param organization 组织名称
     * @param application 应用程序名称
     */
    void initialize(const QString &organization = "EnPlayer", const QString &application = "EnPlayer");
    
    /**
     * @brief 获取Whisper模型路径
     * @return Whisper模型路径
     */
    QString getWhisperPath() const;
    
    /**
     * @brief 设置Whisper模型路径
     * @param path 模型路径
     */
    void setWhisperPath(const QString &path);
    
    /**
     * @brief 获取Whisper模型大小
     * @return 模型大小
     */
    QString getWhisperModelSize() const;
    
    /**
     * @brief 设置Whisper模型大小
     * @param size 模型大小
     */
    void setWhisperModelSize(const QString &size);
    
    /**
     * @brief 获取识别语言
     * @return 语言代码
     */
    QString getRecognitionLanguage() const;
    
    /**
     * @brief 设置识别语言
     * @param language 语言代码
     */
    void setRecognitionLanguage(const QString &language);
    
    /**
     * @brief 获取是否优先使用在线API
     * @return 是否优先使用在线API
     */
    bool isPreferOnlineAPI() const;
    
    /**
     * @brief 设置是否优先使用在线API
     * @param prefer 是否优先使用
     */
    void setPreferOnlineAPI(bool prefer);
    
    /**
     * @brief 获取在线API地址
     * @return API地址
     */
    QString getApiUrl() const;
    
    /**
     * @brief 设置在线API地址
     * @param url API地址
     */
    void setApiUrl(const QString &url);
    
    /**
     * @brief 获取字幕保存目录
     * @return 保存目录
     */
    QString getSubtitleSaveDirectory() const;
    
    /**
     * @brief 设置字幕保存目录
     * @param directory 保存目录
     */
    void setSubtitleSaveDirectory(const QString &directory);
    
    /**
     * @brief 重置所有设置为默认值
     */
    void resetToDefaults();
    
    /**
     * @brief 保存所有设置
     */
    void saveSettings();
    
    /**
     * @brief 加载所有设置
     */
    void loadSettings();

signals:
    /**
     * @brief 设置已更改信号
     */
    void settingsChanged();

private:
    /**
     * @brief 构造函数（私有，单例模式）
     * @param parent 父对象
     */
    explicit SettingsManager(QObject *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~SettingsManager();
    
    QSettings *m_settings; // 设置存储对象
    static SettingsManager *m_instance; // 单例实例
    
    // 设置项
    QString m_whisperPath;         // Whisper模型路径
    QString m_whisperModelSize;    // Whisper模型大小
    QString m_recognitionLanguage; // 识别语言
    bool m_preferOnlineAPI;        // 是否优先使用在线API
    QString m_apiUrl;              // 在线API地址
    QString m_subtitleSaveDirectory; // 字幕保存目录
    
    /**
     * @brief 设置默认值
     */
    void setDefaultValues();
};

#endif // SETTINGSMANAGER_H