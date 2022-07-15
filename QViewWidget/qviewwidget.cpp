#include "qviewwidget.h"

#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DExtras/QForwardRenderer>
#include <Qt3DExtras/QOrbitCameraController>
#include <Qt3DExtras/QTorusMesh>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DInput/QInputAspect>
#include <Qt3DRender/QMesh>
#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QCameraLens>
#include <Qt3DRender/QPointLight>
#include <Qt3DRender/QAttribute>
#include <Qt3DRender/QGeometry>
#include <Qt3DRender/QBuffer>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>

#include <QBoxLayout>
#include <QFileDialog>
#include <QPushButton>
#include <cmath>

QViewWidget::QViewWidget(QWidget *parent) : QWidget(parent)
{
    initializeGL();
    initRemote();
    attachSharedMemory();
}

QViewWidget::~QViewWidget()
{
    ruinRemote();
    detachSharedMemory();
}

void QViewWidget::initializeGL()
{
    auto window = new Qt3DExtras::Qt3DWindow();
    window->defaultFrameGraph()->setClearColor(QColor(QRgb(0x4d4d4f)));

    auto input = new Qt3DInput::QInputAspect;
    window->registerAspect(input);

    auto *camera = window->camera();
    camera->lens()->setPerspectiveProjection(45.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
    camera->setPosition(QVector3D(0, 0, 20.0f));
    camera->setUpVector(QVector3D(0, 1, 0));
    camera->setViewCenter(QVector3D(0, 0, 0));

    auto rootEntity = new Qt3DCore::QEntity();
    auto lightEntity = new Qt3DCore::QEntity(rootEntity);

    auto light = new Qt3DRender::QPointLight(lightEntity);
    light->setColor(QColor("white"));
    light->setIntensity(1);
    lightEntity->addComponent(light);

    auto lightTransform = new Qt3DCore::QTransform(lightEntity);
    lightTransform->setTranslation(camera->position());
    lightEntity->addComponent(lightTransform);

    auto camController = new Qt3DExtras::QOrbitCameraController(rootEntity);
    camController->setCamera(camera);

    m_mesh = new Qt3DRender::QMesh();
    m_mesh->setSource(QUrl("file:///D:/temp/model3.obj"));

    auto *material = new Qt3DExtras::QPhongMaterial();
    material->setDiffuse(QColor(QRgb(0xeeeeee)));

    auto meshTransform = new Qt3DCore::QTransform();
    meshTransform->setScale(0.1f);
    meshTransform->setRotation(QQuaternion::fromAxisAndAngle(QVector3D(0.0f, 0.0f, 0.0f), 25.0f));
    meshTransform->setTranslation(QVector3D(0.0f, 0.0f, 0.0f));

    auto meshEntity = new Qt3DCore::QEntity(rootEntity);
    meshEntity->addComponent(m_mesh);
    meshEntity->addComponent(material);
    meshEntity->addComponent(meshTransform);

    window->setRootEntity(rootEntity);

    QPushButton* button = new QPushButton("memory");
    connect(button, &QPushButton::clicked, this, &QViewWidget::onButton);

    QPushButton* button2 = new QPushButton("websocket");
    connect(button2, &QPushButton::clicked, this, &QViewWidget::onButton2);

    QHBoxLayout* hlayout = new QHBoxLayout();
    hlayout->addWidget(button);
    hlayout->addWidget(button2);

    QVBoxLayout* vlayout = new QVBoxLayout(this);
    vlayout->addWidget(QWidget::createWindowContainer(window));
    vlayout->addLayout(hlayout);

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &QViewWidget::onTimer);
}

void QViewWidget::onSocket(const QString &data)
{
    qDebug() << data;
}

void QViewWidget::onButton()
{
    static QByteArray cellArray;
    static QByteArray pointArray;
    readSharedMemory(cellArray, pointArray);
    float* cellptr = reinterpret_cast<float*>(cellArray.data());
    float* pointptr = reinterpret_cast<float*>(pointArray.data());

    QJsonObject message;
    auto *geometry = m_mesh->geometry();
    for (auto attri : geometry->attributes())
    {
        auto type = attri->attributeType();
        switch (type)
        {
            case Qt3DRender::QAttribute::VertexAttribute:
                {
                    if(attri->name() == "vertexPosition")
                    {
                        auto buf = attri->buffer();
                        auto bufferBytes = buf->data();
                        quint32 pntSize = attri->count();
                        float* fptr = reinterpret_cast<float*>(bufferBytes.data());

                        QVector<float*> normals((int)pntSize, nullptr);

                        QVariantList positionArr;
                        auto faceSize = (uint)cellArray.length() / (sizeof(float) * 3);
                        for (decltype(faceSize) i = 0; i < faceSize; ++i)
                        {
                            qint32 index[3] = {0};
                            index[0] = (qint32)*(cellptr + i*3);
                            index[1] = (qint32)*(cellptr + i*3 + 1);
                            index[2] = (qint32)*(cellptr + i*3 + 2);

                            float *v1 = pointptr + index[0] * 3;
                            float *v2 = pointptr + index[1] * 3;
                            float *v3 = pointptr + index[2] * 3;

                            float ax, ay, az, bx, by, bz;
                            ax = v3[0] - v2[0];
                            ay = v3[1] - v2[1];
                            az = v3[2] - v2[2];
                            bx = v1[0] - v2[0];
                            by = v1[1] - v2[1];
                            bz = v1[2] - v2[2];

                            for(int i = 0; i < 3; i++)
                            {
                                qint32 idx = index[i];
                                if(!normals[idx])
                                {
                                    normals[idx] = new float[3];
                                    normals[idx][0] = (ay * bz - az * by);
                                    normals[idx][1] = (az * bx - ax * bz);
                                    normals[idx][2] = (ax * by - ay * bx);
                                }
                                positionArr.append((int)(pointptr[idx*3]*1000));
                                positionArr.append((int)(pointptr[idx*3+1]*1000));
                                positionArr.append((int)(pointptr[idx*3+2]*1000));
                            }
                        }
                        message.insert("position", QJsonArray::fromVariantList(positionArr));

                        for (decltype(pntSize) i = 0; i < pntSize; ++i)
                        {
                            *fptr++ = *pointptr++;
                            *fptr++ = *pointptr++;
                            *fptr++ = *pointptr++;

                            float *n = normals[i];
                            if(n)
                            {
                                *fptr++ = n[0];
                                *fptr++ = n[1];
                                *fptr++ = n[2];
                            }
                            else {
                                fptr += 3;
                            }
                        }
                        qDeleteAll(normals);
                        buf->setData(bufferBytes);
                    }
                    break;
                }
            case Qt3DRender::QAttribute::IndexAttribute:
                {
                    if(cellArray.isEmpty())
                    {
                        break;
                    }
                    auto buf = attri->buffer();
                    auto indexBytes = buf->data();
                    auto faceSize = (uint)indexBytes.size() / (sizeof(quint32) * 3);
                    quint32* usptr = reinterpret_cast<quint32*>(indexBytes.data());

                    for (decltype(faceSize) i = 0; i < faceSize; ++i)
                    {
                        *usptr++ = (quint32)*cellptr++;
                        *usptr++ = (quint32)*cellptr++;
                        *usptr++ = (quint32)*cellptr++;
                    }
                    buf->setData(indexBytes);
                    break;
                }
            case Qt3DRender::QAttribute::DrawIndirectAttribute:
                break;
            default:
                break;
        }
    }
    if(message.count())
    {
        remoteSend(message);
    }
}

void QViewWidget::onButton2()
{
    if(m_timer->isActive())
    {
        m_timer->stop();
    }
    else {
        m_timer->start(1000 / 15);
    }
    qDebug() << "onButton2";
}

void QViewWidget::onTimer()
{
    onButton();
}

void QViewWidget::initRemote()
{
    m_webSocket = new QWebSocket();
    m_webSocket->open(QUrl("ws://localhost:8081/model/gui"));

//    QObject::connect(m_webSocket, &QWebSocket::connected, [&](){
//        qDebug() << "connected";
//        m_remote = 1;
//    });

//    QObject::connect(m_webSocket, &QWebSocket::disconnected, [&](){
//        qDebug() << "disconnected";
//        m_remote = 0;
//    });

    QObject::connect(m_webSocket, &QWebSocket::textMessageReceived, [&](const QString &message){
        onSocket(message);
    });
}

void QViewWidget::ruinRemote()
{
    m_webSocket->close();
    m_webSocket->deleteLater();
    m_webSocket = nullptr;
}

void QViewWidget::remoteSend(QJsonObject &data)
{
    QJsonDocument doc;
    doc.setObject(data);
    m_webSocket->sendBinaryMessage(doc.toJson(QJsonDocument::Compact));
}

void QViewWidget::attachSharedMemory()
{
    if (!m_sharedMemory)
    {
        m_sharedMemory = new QSharedMemory("MateFacePointsMemory");
        m_sharedMemory->attach();
    }
}

void QViewWidget::detachSharedMemory()
{
    if (m_sharedMemory)
    {
        m_sharedMemory->detach();
        m_sharedMemory->deleteLater();
    }
}

void QViewWidget::readSharedMemory(QByteArray &cellArray, QByteArray &pointArray)
{
    if(!m_sharedMemory->isAttached())
    {
        m_sharedMemory->attach();
    }
    m_sharedMemory->lock();
    char* form = (char*)m_sharedMemory->data();

    int *len = (int*)form;
    form += sizeof(int);
    if(*len) cellArray = QByteArray((const char*)form, *len);
    form += *len;

    int *len2 = (int*)form;
    form += sizeof(int);
    if(*len2) pointArray = QByteArray((const char*)form, *len2);
    form += *len2;
    m_sharedMemory->unlock();
}
