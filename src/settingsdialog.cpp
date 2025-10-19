#include "settingsdialog.h"
#include "../forms/ui_settingsdialog.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QDebug>

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog),
    m_settingsManager(SettingsManager::instance())
{
    ui->setupUi(this);
    initUI();
    loadSettingsToUI();
    updateControlStates();
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::initUI()
{
    // 设置对话框为模态
    this->setWindowModality(Qt::ApplicationModal);
    
    // 设置固定大小
    this->setFixedSize(this->size());
    
    // 连接信号槽
    connect(ui->preferOnlineApiCheckBox, &QCheckBox::toggled, this, &SettingsDialog::on_preferOnlineApiCheckBox_toggled);
}

void SettingsDialog::loadSettingsToUI()
{
    // 加载Whisper设置
    ui->whisperPathLineEdit->setText(m_settingsManager->getWhisperPath());
    ui->modelSizeComboBox->setCurrentText(m_settingsManager->getWhisperModelSize());
    ui->languageComboBox->setCurrentText(m_settingsManager->getRecognitionLanguage());
    ui->preferOnlineApiCheckBox->setChecked(m_settingsManager->isPreferOnlineAPI());
    ui->apiUrlLineEdit->setText(m_settingsManager->getApiUrl());
    
    // 加载字幕设置
    ui->subtitleDirLineEdit->setText(m_settingsManager->getSubtitleSaveDirectory());
}

void SettingsDialog::saveSettingsFromUI()
{
    // 保存Whisper设置
    m_settingsManager->setWhisperPath(ui->whisperPathLineEdit->text());
    m_settingsManager->setWhisperModelSize(ui->modelSizeComboBox->currentText());
    m_settingsManager->setRecognitionLanguage(ui->languageComboBox->currentText());
    m_settingsManager->setPreferOnlineAPI(ui->preferOnlineApiCheckBox->isChecked());
    m_settingsManager->setApiUrl(ui->apiUrlLineEdit->text());
    
    // 保存字幕设置
    m_settingsManager->setSubtitleSaveDirectory(ui->subtitleDirLineEdit->text());
    
    // 保存到文件
    m_settingsManager->saveSettings();
}

void SettingsDialog::updateControlStates()
{
    // 根据是否优先使用在线API来启用/禁用相关控件
    bool preferOnline = ui->preferOnlineApiCheckBox->isChecked();
    
    // 本地Whisper设置控件
    ui->whisperPathLineEdit->setEnabled(!preferOnline);
    ui->browseWhisperPathButton->setEnabled(!preferOnline);
    ui->modelSizeComboBox->setEnabled(!preferOnline);
    ui->downloadModelButton->setEnabled(!preferOnline);
    
    // 在线API设置控件
    ui->apiUrlLineEdit->setEnabled(preferOnline);
}

void SettingsDialog::on_browseWhisperPathButton_clicked()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        tr("选择Whisper模型文件"),
        QString(),
        tr("Whisper模型文件 (*.bin *.pt *.en.pt *.ggml *.ggmlv3);;所有文件 (*.*)")
    );
    
    if (!path.isEmpty()) {
        ui->whisperPathLineEdit->setText(path);
    }
}

void SettingsDialog::on_browseSubtitleDirButton_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("选择字幕保存目录"),
        ui->subtitleDirLineEdit->text()
    );
    
    if (!dir.isEmpty()) {
        ui->subtitleDirLineEdit->setText(dir);
    }
}

void SettingsDialog::on_downloadModelButton_clicked()
{
    // 模型下载提示，提供更多信息
    QString modelSize = ui->modelSizeComboBox->currentText();
    QMessageBox::information(
        this,
        tr("模型下载"),
        tr("您可以通过以下方式下载Whisper %1模型：\n\n").arg(modelSize) +
        tr("1. 使用whisper目录下的脚本：\n") +
        tr("   cd whisper && ./models/download-ggml-model.sh %1\n\n").arg(modelSize) +
        tr("2. 或者访问Whisper官方仓库手动下载：\n") +
        tr("   https://github.com/ggerganov/whisper.cpp/tree/master/models\n\n") +
        tr("下载完成后，在此对话框中选择模型文件(.bin格式)"),
        QMessageBox::Ok
    );
}

void SettingsDialog::on_applyButton_clicked()
{
    // 保存设置
    saveSettingsFromUI();
    
    // 显示成功消息
    QMessageBox::information(this, tr("成功"), tr("设置已应用"));
}

void SettingsDialog::on_okButton_clicked()
{
    // 保存设置
    saveSettingsFromUI();
    
    // 接受对话框
    accept();
}

void SettingsDialog::on_cancelButton_clicked()
{
    // 拒绝对话框
    reject();
}

void SettingsDialog::on_resetButton_clicked()
{
    // 确认重置
    int ret = QMessageBox::question(
        this,
        tr("确认重置"),
        tr("确定要将所有设置重置为默认值吗？这将丢失您的自定义设置。"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );
    
    if (ret == QMessageBox::Yes) {
        // 重置设置
        m_settingsManager->resetToDefaults();
        
        // 更新UI
        loadSettingsToUI();
        updateControlStates();
        
        // 显示成功消息
        QMessageBox::information(this, tr("成功"), tr("设置已重置为默认值"));
    }
}

void SettingsDialog::on_preferOnlineApiCheckBox_toggled(bool checked)
{
    // 更新控件状态
    updateControlStates();
}