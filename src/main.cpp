#include "mainwindow.h"
#include <QApplication>
#include <QTime>
#include <QTextCodec>
#include <QDebug>
#include <QMessageLogContext>

// 全局MainWindow指针，用于在消息处理器中访问
MainWindow *globalMainWindow = nullptr;

// 自定义消息处理器，将Qt日志消息重定向到MainWindow的日志系统
void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // 确定日志级别
    QString level;
    switch (type) {
    case QtDebugMsg:
        level = "DEBUG";
        break;
    case QtInfoMsg:
        level = "INFO";
        break;
    case QtWarningMsg:
        level = "WARNING";
        break;
    case QtCriticalMsg:
        level = "ERROR";
        break;
    case QtFatalMsg:
        level = "FATAL";
        break;
    default:
        level = "UNKNOWN";
    }
    
    // 提取文件名（去掉路径）
    QString fileName = QString(context.file).section('/', -1);
    
    // 如果消息包含[CRITICAL]前缀，确保使用ERROR级别
    QString displayMsg = msg;
    if (msg.contains("[CRITICAL]")) {
        level = "ERROR";
    }
    
    // 格式化消息，添加文件和行号信息（如果可用）
    if (context.file && context.line > 0) {
        displayMsg = QString("%1 (%2:%3)").arg(displayMsg).arg(fileName).arg(context.line);
    }
    
    // 使用全局MainWindow指针将消息添加到UI
    if (globalMainWindow) {
        globalMainWindow->logMessage(displayMsg, level);
    }
    
    // 同时输出到控制台，保持原有的控制台输出功能
    QByteArray localMsg = msg.toLocal8Bit();
    switch (type) {
    case QtDebugMsg:
        fprintf(stderr, "DEBUG: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
    case QtInfoMsg:
        fprintf(stderr, "INFO: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
    case QtWarningMsg:
        fprintf(stderr, "WARNING: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
    case QtCriticalMsg:
        fprintf(stderr, "CRITICAL: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
    case QtFatalMsg:
        fprintf(stderr, "FATAL: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        abort();
    }
}

int main(int argc, char *argv[])
{
    // 设置全局字符编码为UTF-8，确保控制台输出能正确处理中文字符
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
    
    // 设置随机数种子
    qsrand(static_cast<uint>(QTime::currentTime().msec()));
    
    // 安装自定义消息处理器
    qInstallMessageHandler(customMessageHandler);
    
    QApplication a(argc, argv);
    
    // 设置应用程序信息
    a.setApplicationName("EnPlayer");
    a.setApplicationVersion("1.0");
    
    MainWindow w;
    globalMainWindow = &w; // 设置全局MainWindow指针
    w.show();
    
    int result = a.exec();
    
    // 应用程序退出前清理全局指针
    globalMainWindow = nullptr;
    
    return result;
}
