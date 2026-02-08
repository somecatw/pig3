#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "mathbase.h"
#include "structures.h"
#include "assetmanager.h"
#include "utils.h"
#include <QKeyEvent>

using namespace std;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    stage = new Stage3D(this);
    t=0;
    this->setCentralWidget(stage);

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateFrame);
    timer->start(std::chrono::milliseconds(16));
    // assetManager.loadOBJ("D:\\project\\qt_c++\\pig3\\assets\\wuqie.obj");
    // assetManager.loadOBJ("..\\..\\assets\\dust2\\de_dust2.obj");
    assetManager.loadOBJ("..\\..\\assets\\dust3\\part10.obj");

    GameObject *ttfa = new GameObject(stage->root);
    for(auto [id, m]: enumerate(assetManager.getMeshes())){
        new MeshActor(id, ttfa);
    }
    CameraInfo info;
    info.focalLength = 4.0f;
    info.frame.axisX = {1, 0, 0};
    info.frame.axisY = {0, 1, 0};
    info.frame.axisZ = {0, 0, 1};
    info.width = 15;
    info.height = 10;
    info.screenSize = {8, 5.6, 0};
    camera = new Camera(info, stage->root);
    stage->activeCam = camera;


    Transform rot = Transform::rotateAroundAxis({1, 0, 0}, -1.57) * Transform::translate({0, 0, -400});
    ttfa->setTransform(rot);
    for(Mesh &m:assetManager.getMeshes()){
        // m.applyTransform(rot);
        m.shaderConfig |= ShaderConfig::DisableLightModel;
    }
    fpsLabel = new QLabel();
    fpsLabel->setFont(QFont("Times New Roman", 15));
    fpsLabel->setText("114514");
    ui->statusbar->insertWidget(0, fpsLabel);

    memset(wasdFlags, 0, 4u);
    startPoint = {0,0};
    xacc = yacc = 0;

}

void MainWindow::updateFrame(){
    if(wasdFlags[0]){
        camera->localTranslate({0, 0, 100});
    }
    if(wasdFlags[1]){
        camera->localTranslate({-100, 0, 0});
    }
    if(wasdFlags[2]){
        camera->localTranslate({0, 0, -100});
    }
    if(wasdFlags[3]){
        camera->localTranslate({100, 0, 0});
    }
    if(dragFlag){
        camera->rotateAroundAxis(camera->getTransform().rotation.row(0), 0.01*yacc);
        camera->rotateAroundAxis({0,1,0}, -0.01*xacc);
        xacc = yacc = 0;
    }

    fpsLabel->setText(QString::asprintf("%.1f fps (render %.0f us, total %.0f us)", 1e6 / stage->avgFrameTime, stage->avgRenderTime, stage->avgFrameTime));

    stage->updateFrame();
    return;
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
