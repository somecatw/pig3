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

    vector<Vertex> vertices = ttfa.vertices;
    vector<Triangle> triangles = ttfa.triangles;
    // for(auto &t:triangles)
    //    t.shaderConfig |= ShaderConfig::WireframeOnly;

    // vector<Vertex> vertices = {{{0,0,0},{0,0,0}},{{0,0,2},{0,0,0}},{{0,2,0},{0,0,0}},{{0,2,2},{0,0,0}},
    //                            {{2,0,0},{0,0,0}},{{2,0,2},{0,0,0}},{{2,2,0},{0,0,0}},{{2,2,2},{0,0,0}}};
    // vector<Triangle> triangles = {{0,1,2},{1,2,3},{0,4,6},{0,2,6},{4,5,6},{5,6,7},{0,4,5},{0,1,5},{2,6,3},{3,6,7},{1,3,5},{3,5,7}};
    //vector<Triangle> triangles = {{3,6,7}};

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
    drawFrame(vertices, triangles, cam, ptr);
    label->setPixmap(QPixmap::fromImage(*img));

}


MainWindow::~MainWindow()
{
    delete ui;
}
