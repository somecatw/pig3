#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "mathbase.h"
#include "structures.h"
#include "render.h"
#include "assetmanager.h"
#include <QKeyEvent>

using namespace std;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    label=new QLabel(this);
    t=0;
    this->setCentralWidget(label);
    // img = new QImage(640, 448, QImage::Format_ARGB32);
    img = new QImage(960, 640, QImage::Format_ARGB32);
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateFrame);
    timer->start(std::chrono::milliseconds(16));
    // assetManager.loadOBJ("D:\\project\\qt_c++\\pig3\\assets\\wuqie.obj");
    // assetManager.loadOBJ("..\\..\\assets\\dust2\\de_dust2.obj");
    assetManager.loadOBJ("..\\..\\assets\\dust3\\part10.obj");

    Transform rot = Transform::rotateAroundAxis({1, 0, 0}, -1.57) * Transform::translate({0, 0, -400});
    for(Mesh &m:assetManager.getMeshes()){
        m.applyTransform(rot);
        m.shaderConfig |= ShaderConfig::DisableLightModel;
    }
    fpsLabel = new QLabel();
    fpsLabel->setFont(QFont("Times New Roman", 15));
    fpsLabel->setText("114514");
    ui->statusbar->insertWidget(0, fpsLabel);
    // exit(0);
    // ttfa = assetManager.getMeshes()[1];
    memset(wasdFlags, 0, 4u);
    startPoint = {0,0};
    xacc = yacc = 0;
}

void MainWindow::updateFrame(){
    uint *ptr=(uint*)img->bits();
    t+=2;
    // ttfa.applyTransform(Transform::rotateAroundAxis({0,1,0}, 0.015));
    // ttfa.shaderConfig = ShaderConfig::DisableBackCulling | ShaderConfig::WireframeOnly;

    clearRenderBuffer();
    // submitMesh(ttfa);
    for(Mesh &m:assetManager.getMeshes()){
        // m.applyTransform(Transform::translate({0, 5, 0}));S
        submitMesh(m);
    }
    fpsLabel ->setText(QString::number(frameStat.fps)+" fps");

    if(wasdFlags[0]){
        camTrans = camTrans * Transform::translate({0, 0, 100});
    }
    if(wasdFlags[1]){
        camTrans = camTrans * Transform::translate({-100, 0, 0});
    }
    if(wasdFlags[2]){
        camTrans = camTrans * Transform::translate({0, 0, -100});
    }
    if(wasdFlags[3]){
        camTrans = camTrans * Transform::translate({100, 0, 0});
    }
    if(dragFlag){
        camTrans = camTrans
                   * Transform::rotateAroundAxis(camTrans.rotation.row(0), 0.01*yacc)
                   * Transform::rotateAroundAxis({0,1,0}, -0.01*xacc);
        xacc = yacc = 0;
    }

    CameraInfo cam;
    cam.focalLength = 4.0f;
    float yy;

    yy = 280 + 150 * min(max(0.0f, t-450.0f)/450, 1.0f);
    cam.pos = {float(1600-t), yy, 2150};
    cam.pos = camTrans.translation;

    // 带光影版的参数
    // yy = -1000 + 2400 * min(max(0.0f, t*10-8000.0f) / 8000, 1.0f);
    // cam.pos = {float(26500-t*10), yy, 36000};

    // cam.pos = {0, 2, -24};
    cam.width = 15;
    cam.height = 10;
    // cam.frame.axisX = {sqrt(2.0f)/2, 0, sqrt(2.0f)/2};
    cam.frame.axisX = {0.5, 0, sqrt(3.0f)/2};
    // cam.frame.axisX = {1, 0, 0};
    cam.frame.axisY = {0, 1, 0};

    cam.frame.axisX = camTrans.rotation.row(0);
    cam.frame.axisY = camTrans.rotation.row(1);
    cam.frame.axisZ = camTrans.rotation.row(2);

    cam.frame.axisZ = cam.frame.axisX.cross(cam.frame.axisY);
    qDebug()<<cam.frame.axisZ.to_string();

    cam.screenSize = {8, 5.6, 0};
    drawFrame(cam, ptr);
    label->setPixmap(QPixmap::fromImage(*img));
    // label->setPixmap(QPixmap::fromImage(assetManager.getMaterials()[6].mipmaps[0]));
}


MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::keyPressEvent(QKeyEvent *evt){
    if(evt->key() == Qt::Key_W){
        wasdFlags[0] = true;
    }
    if(evt->key() == Qt::Key_A){
        wasdFlags[1] = true;
    }
    if(evt->key() == Qt::Key_S){
        wasdFlags[2] = true;
    }
    if(evt->key() == Qt::Key_D){
        wasdFlags[3] = true;
    }
}

void MainWindow::keyReleaseEvent(QKeyEvent *evt){
    if(evt->key() == Qt::Key_W){
        wasdFlags[0] = false;
    }
    if(evt->key() == Qt::Key_A){
        wasdFlags[1] = false;
    }
    if(evt->key() == Qt::Key_S){
        wasdFlags[2] = false;
    }
    if(evt->key() == Qt::Key_D){
        wasdFlags[3] = false;
    }
}

void MainWindow::mousePressEvent(QMouseEvent *evt){
    dragFlag = true;
    startPoint = evt->pos();
    xacc = yacc = 0;
}

void MainWindow::mouseMoveEvent(QMouseEvent *evt){
    dragFlag = true;
    xacc += (evt->pos() - startPoint).x();
    yacc += (evt->pos() - startPoint).y();
    startPoint = evt->pos();
}

void MainWindow::mouseReleaseEvent(QMouseEvent *evt){
    dragFlag = false;
}
