#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include "settingsmanager.h"

namespace Ui {
class SettingsDialog;
}

/**
 * @brief 设置对话框类
 * 
 * 提供用户界面来配置应用程序的设置
 */
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口
     */
    explicit SettingsDialog(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~SettingsDialog();

private slots:
    /**
     * @brief 浏览Whisper模型路径按钮点击
     */
    void on_browseWhisperPathButton_clicked();
    
    /**
     * @brief 浏览字幕保存目录按钮点击
     */
    void on_browseSubtitleDirButton_clicked();
    
    /**
     * @brief 下载模型按钮点击
     */
    void on_downloadModelButton_clicked();
    
    /**
     * @brief 应用按钮点击
     */
    void on_applyButton_clicked();
    
    /**
     * @brief 确定按钮点击
     */
    void on_okButton_clicked();
    
    /**
     * @brief 取消按钮点击
     */
    void on_cancelButton_clicked();
    
    /**
     * @brief 重置按钮点击
     */
    void on_resetButton_clicked();
    
    /**
     * @brief 优先使用在线API选项变更
     * @param checked 是否选中
     */
    void on_preferOnlineApiCheckBox_toggled(bool checked);

private:
    Ui::SettingsDialog *ui; // UI对象
    SettingsManager *m_settingsManager; // 设置管理器
    
    /**
     * @brief 初始化UI
     */
    void initUI();
    
    /**
     * @brief 从设置管理器加载设置到UI
     */
    void loadSettingsToUI();
    
    /**
     * @brief 从UI保存设置到设置管理器
     */
    void saveSettingsFromUI();
    
    /**
     * @brief 检查并启用/禁用相关控件
     */
    void updateControlStates();
};

#endif // SETTINGSDIALOG_H