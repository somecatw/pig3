#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "mathbase.h"
#include "structures.h"
#include "render.h"
#include "assetmanager.h"

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
    assetManager.loadOBJ("D:\\project\\qt_c++\\pig3\\assets\\dust2\\de_dust2.obj");

    Transform rot = Transform::rotateAroundAxis({1, 0, 0}, -1.57) * Transform::translate({0, 0, -400});
    for(Mesh &m:assetManager.getMeshes()){
        m.applyTransform(rot);
        m.shaderConfig |= ShaderConfig::DisableLightModel;
    }
    // exit(0);
    // ttfa = assetManager.getMeshes()[1];
}

void MainWindow::updateFrame(){
    uint *ptr=(uint*)img->bits();

    // ttfa.applyTransform(Transform::rotateAroundAxis({0,1,0}, 0.015));
    // ttfa.shaderConfig = ShaderConfig::DisableBackCulling | ShaderConfig::WireframeOnly;

    clearRenderBuffer();
    // submitMesh(ttfa);
    for(Mesh &m:assetManager.getMeshes()){
        // m.applyTransform(Transform::translate({0, 5, 0}));
        submitMesh(m);
    }

    CameraInfo cam;
    cam.focalLength = 4.0f;
    cam.pos = {1700, 200, 500};
    // cam.pos = {0, 2, -12};
    cam.width = 15;
    cam.height = 10;
    cam.frame.axisX = {sqrt(2.0f)/2, 0, sqrt(2.0f)/2};
    // cam.frame.axisX = {1, 0, 0};
    cam.frame.axisY = {0, 1, 0};
    // cam.frame.axisX = {0.98, 0, sqrt(0.0396f)};
    // cam.frame.axisY = {0, 1, 0};
    cam.frame.axisZ = cam.frame.axisX.cross(cam.frame.axisY);
    qDebug()<<cam.frame.axisZ.to_string();
    cam.screenSize = {8, 5.6, 0};
    drawFrame(cam, ptr);
    label->setPixmap(QPixmap::fromImage(*img));

}


MainWindow::~MainWindow()
{
    delete ui;
}
