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
    img = new QImage(640, 448, QImage::Format_ARGB32);
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateFrame);
    timer->start(std::chrono::milliseconds(16));
    assetManager.loadOBJ("D:\\project\\qt_c++\\pig3\\assets\\wuqie.obj");
    ttfa = assetManager.getMeshes()[0];
}

void MainWindow::updateFrame(){
    uint *ptr=(uint*)img->bits();

    ttfa.applyTransform(Transform::rotateAroundAxis({0,1,0}, 0.015));
    // ttfa.shaderConfig = ShaderConfig::DisableBackCulling | ShaderConfig::WireframeOnly;

    clearRenderBuffer();
    submitMesh(ttfa);

    CameraInfo cam;
    cam.focalLength = 4.0f;
    cam.pos = {0, 6, -8};
    cam.width = 10;
    cam.height = 7;
    cam.frame.axisX = {1, 0, 0};
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
