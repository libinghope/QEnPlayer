#include "settingsmanager.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

// 静态实例初始化
SettingsManager *SettingsManager::m_instance = nullptr;

SettingsManager::SettingsManager(QObject *parent) : QObject(parent)
{
    m_settings = nullptr;
    setDefaultValues();
}

SettingsManager::~SettingsManager()
{
    if (m_settings) {
        delete m_settings;
        m_settings = nullptr;
    }
}

SettingsManager* SettingsManager::instance()
{
    if (!m_instance) {
        m_instance = new SettingsManager();
    }
    return m_instance;
}

void SettingsManager::initialize(const QString &organization, const QString &application)
{
    if (m_settings) {
        delete m_settings;
    }
    
    m_settings = new QSettings(organization, application);
    loadSettings();
}

void SettingsManager::setDefaultValues()
{
    m_whisperPath = "";
    m_whisperModelSize = "small";
    m_recognitionLanguage = "auto";
    m_preferOnlineAPI = false;
    m_apiUrl = "https://api.example.com/asr";
    
    // 默认字幕保存目录为用户的文档目录
    m_subtitleSaveDirectory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QDir::separator() + "EnPlayer" + QDir::separator() + "Subtitles";
    
    // 确保默认目录存在
    QDir dir(m_subtitleSaveDirectory);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
}

QString SettingsManager::getWhisperPath() const
{
    return m_whisperPath;
}

void SettingsManager::setWhisperPath(const QString &path)
{
    if (m_whisperPath != path) {
        m_whisperPath = path;
        emit settingsChanged();
    }
}

QString SettingsManager::getWhisperModelSize() const
{
    return m_whisperModelSize;
}

void SettingsManager::setWhisperModelSize(const QString &size)
{
    if (m_whisperModelSize != size) {
        m_whisperModelSize = size;
        emit settingsChanged();
    }
}

QString SettingsManager::getRecognitionLanguage() const
{
    return m_recognitionLanguage;
}

void SettingsManager::setRecognitionLanguage(const QString &language)
{
    if (m_recognitionLanguage != language) {
        m_recognitionLanguage = language;
        emit settingsChanged();
    }
}

bool SettingsManager::isPreferOnlineAPI() const
{
    return m_preferOnlineAPI;
}

void SettingsManager::setPreferOnlineAPI(bool prefer)
{
    if (m_preferOnlineAPI != prefer) {
        m_preferOnlineAPI = prefer;
        emit settingsChanged();
    }
}

QString SettingsManager::getApiUrl() const
{
    return m_apiUrl;
}

void SettingsManager::setApiUrl(const QString &url)
{
    if (m_apiUrl != url) {
        m_apiUrl = url;
        emit settingsChanged();
    }
}

QString SettingsManager::getSubtitleSaveDirectory() const
{
    return m_subtitleSaveDirectory;
}

void SettingsManager::setSubtitleSaveDirectory(const QString &directory)
{
    if (m_subtitleSaveDirectory != directory) {
        m_subtitleSaveDirectory = directory;
        
        // 确保目录存在
        QDir dir(directory);
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        
        emit settingsChanged();
    }
}

void SettingsManager::saveSettings()
{
    if (!m_settings) {
        qWarning() << "SettingsManager not initialized";
        return;
    }
    
    m_settings->beginGroup("Whisper");
    m_settings->setValue("Path", m_whisperPath);
    m_settings->setValue("ModelSize", m_whisperModelSize);
    m_settings->setValue("Language", m_recognitionLanguage);
    m_settings->setValue("PreferOnlineAPI", m_preferOnlineAPI);
    m_settings->setValue("ApiUrl", m_apiUrl);
    m_settings->endGroup();
    
    m_settings->beginGroup("Subtitles");
    m_settings->setValue("SaveDirectory", m_subtitleSaveDirectory);
    m_settings->endGroup();
    
    // 确保保存设置
    m_settings->sync();
    
    qDebug() << "Settings saved successfully";
}

void SettingsManager::loadSettings()
{
    if (!m_settings) {
        qWarning() << "SettingsManager not initialized";
        setDefaultValues();
        return;
    }
    
    // 如果没有设置文件，使用默认值
    if (m_settings->allKeys().isEmpty()) {
        qDebug() << "No settings file found, using default values";
        setDefaultValues();
        return;
    }
    
    m_settings->beginGroup("Whisper");
    m_whisperPath = m_settings->value("Path", "").toString();
    m_whisperModelSize = m_settings->value("ModelSize", "small").toString();
    m_recognitionLanguage = m_settings->value("Language", "auto").toString();
    m_preferOnlineAPI = m_settings->value("PreferOnlineAPI", false).toBool();
    m_apiUrl = m_settings->value("ApiUrl", "https://api.example.com/asr").toString();
    m_settings->endGroup();
    
    m_settings->beginGroup("Subtitles");
    m_subtitleSaveDirectory = m_settings->value("SaveDirectory", 
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QDir::separator() + "EnPlayer" + QDir::separator() + "Subtitles").toString();
    m_settings->endGroup();
    
    // 确保字幕目录存在
    QDir dir(m_subtitleSaveDirectory);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    qDebug() << "Settings loaded successfully";
}

void SettingsManager::resetToDefaults()
{
    setDefaultValues();
    saveSettings();
    emit settingsChanged();
}