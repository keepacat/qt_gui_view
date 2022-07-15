#ifndef QVIEWWIDGET_H
#define QVIEWWIDGET_H

#include <QWidget>
#include <QWebSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <Qt3DRender/QMesh>
#include <QSharedMemory>
#include <QTimer>

class QViewWidget : public QWidget
{
    Q_OBJECT

public:
    QViewWidget(QWidget *parent=nullptr);
    ~QViewWidget();

    void initializeGL();

public slots:
    void onSocket(const QString& data);
    void onButton();
    void onButton2();
    void onTimer();

public:
    void initRemote();
    void ruinRemote();
    void remoteSend(QJsonObject &data);

private:
    void attachSharedMemory();
    void detachSharedMemory();
    void readSharedMemory(QByteArray &cellArray, QByteArray &pointArray);
    QSharedMemory *m_sharedMemory=nullptr;

private:
    int m_remote;
    QTimer *m_timer;
    Qt3DRender::QMesh* m_mesh;
    QWebSocket  *m_webSocket;
};

#endif // QVIEWWIDGET_H
