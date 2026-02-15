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
    assetManager.loadOBJ(".\\assets\\dust3\\part10.obj");

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
    camera->translate({-4000,-2500,-15000});


    Transform rot = Transform::rotateAroundAxis({1, 0, 0}, -1.57) * Transform::translate({0, 0, -400});
    ttfa->setTransform(rot);
    for(Mesh &m:assetManager.getMeshes()){
        // m.applyTransform(rot);
        m.shaderConfig |= ShaderConfig::DisableLightModel;
    }
    stage->buildStaticBVH();
    fpsLabel = new QLabel();
    fpsLabel->setFont(QFont("Times New Roman", 15));
    fpsLabel->setText("114514");
    ui->statusbar->insertWidget(0, fpsLabel);

    memset(wasdFlags, 0, 4u);
    startPoint = {0,0};
    xacc = yacc = 0;
    jumpFlag = false;
}

Vec3 getNormal(const Mesh &mesh, uint triangleID){
    Vec3 v0 = mesh.vertices[mesh.triangles[triangleID].vid[0]].pos;
    Vec3 v1 = mesh.vertices[mesh.triangles[triangleID].vid[1]].pos;
    Vec3 v2 = mesh.vertices[mesh.triangles[triangleID].vid[2]].pos;
    return (v1-v0).cross(v2-v0).normalized();
}
const float movSpeed = 80;
const float charaHeight = 1000;
const float liftThreshold = 325;

BBox3D charaBox(const Vec3 &pos, bool extraY=false){
    BBox3D box;
    box.x1 = pos.x - 200;
    box.x2 = pos.x + 200;
    box.y1 = pos.y - 10;
    box.y2 = pos.y + 1000 + (extraY?100:0);
    box.z1 = pos.z - 200;
    box.z2 = pos.z + 200;
    return box;
}
Vec3 posIterate(Stage3D *stage, Vec3 pos, Vec3 mov){
    Vec3 unit = mov.normalized();
    // mov = movSpeed * unit;
    // SceneRayHit movHit = stage->raytest({pos + Vec3({0, charaHeight-liftThreshold, 0}), unit});
    // if(movHit.actor != nullptr && movHit.dis < mov.len()){
    //     Vec3 normal = getNormal(movHit.actor->mesh, movHit.triangleID);
    //     Vec3 tangent = normal.cross({0, 1, 0});
    //     float ttfa = mov.dot(tangent);
    //     mov = tangent * ttfa;
    // }
    set<BoxtestResult> hit = stage->boxtest(charaBox(pos + mov + Vec3({0, -liftThreshold, 0}), false));
    for(const auto r:hit){
        if(r.normal.dot({0, -1, 0}) < 0.7f && r.normal.dot(unit) <= 0.0f){
            Vec3 tangent = r.normal.cross({0, 1, 0}).normalized();
            float ttfa = mov.dot(tangent);
            mov = tangent * ttfa;
            unit = mov.normalized();
            // break;
            qDebug()<<tangent.to_string()<<mov.to_string();
        }
    }
    return mov;
}
bool falling;
float ySpeed;

void MainWindow::updateFrame(){
    if(dragFlag){
        camera->rotateAroundAxis({0,1,0}, -0.01*xacc);
        Transform temp = camera->getTransform();
        camera->rotateAroundAxis(camera->getTransform().rotation.row(0), 0.01*yacc);
        if(camera->getGlobalTransform().rotation.row(1).y <= 0) camera->setTransform(temp);
        xacc = yacc = 0;
    }

    Vec3 vx = camera->camInfo.frame.axisX;
    Vec3 vy = camera->camInfo.frame.axisY;
    Vec3 vz = camera->camInfo.frame.axisZ;

    Vec3 front = (vz - Vec3({0, 1, 0}) * Vec3({0, 1, 0}).dot(vz)).normalized();
    Vec3 left = front.cross({0, 1, 0});
    Vec3 mov = {0, 0, 0};
    Vec3 pos = camera->camInfo.pos;

    if(wasdFlags[0]){
        // qDebug()<<front.to_string();
        mov += front;
    }
    if(wasdFlags[1]){
        mov += left;
    }
    if(wasdFlags[2]){
        mov -= front;
    }
    if(wasdFlags[3]){
        mov -= left;
    }
    if(mov.len() > 1e-5){
        mov = mov.normalized() * movSpeed;
        mov = posIterate(stage, pos, mov);
        // mov = posIterate(stage, pos, mov);
        set<BoxtestResult> res = stage->boxtest(charaBox(pos+mov+Vec3{0, -liftThreshold,0}));
        bool wallflag = false;
        // qDebug()<<"mov:"<<mov.to_string();
        for(const auto &r:res){
            qDebug()<<r.normal.to_string();
            if(abs(r.normal.dot({0, -1, 0})) < 0.7 && r.normal.dot(mov.normalized()) < -0.01f) wallflag = true;
        }
        // qDebug()<<wallflag;
        // SceneRayHit wHit = stage->raytest({pos, mov.normalized()});
        // if(wHit.actor != nullptr && wHit.dis < mov.len()) mov = 0.0*(wHit.pos - pos);
        if(wallflag) mov *= 0.0f;
        pos += mov;
    }
    if(jumpFlag && !falling){
        falling = true;
        jumpFlag = false;
        ySpeed = -105;
    }

    BBox3D box = charaBox(pos, true);
    auto boxRes = stage->boxtest(box);
    float groundYMin = 1e9;
    bool groundHit = false;
    for(const auto &r:boxRes){
        if(r.normal.dot({0, -1, 0}) > 0.7f){
            groundYMin = min(groundYMin, r.ymin);
            groundHit = true;
        }
    }

    SceneRayHit upHit = stage->raytest({pos, {0, -1, 0}});
    SceneRayHit downHit = stage->raytest({pos, {0, 1, 0}});

    if(ySpeed < 0.0f && !upHit.miss() && !downHit.miss() && upHit.dis < 100){
        ySpeed = 0.0f;
        camera->moveToLocalPos(upHit.pos+Vec3({0, 1, 0}));
    }else if(ySpeed >= 0.0f && groundHit /*!downHit.miss() && downHit.dis > charaHeight - liftThreshold && downHit.dis < charaHeight + 100*/){
        falling = false;
        ySpeed = 0;
        // camera->moveToLocalPos(downHit.pos + Vec3({0, -charaHeight, 0}));
        // qDebug()<<"hit:"<<downHit.pos.y - charaHeight;
        camera->moveToLocalPos({pos.x, groundYMin - charaHeight, pos.z});
    }else{
        falling = true;
        ySpeed += 6;
        // if(downHit.actor != nullptr && upHit.dis + downHit.dis > charaHeight + 100)
        camera->translate(mov);
        camera->translate({0, ySpeed, 0});
    }

    fpsLabel->setText(QString::asprintf("%.1f fps (render %.0f us, total %.0f us)", 1e6 / stage->avgFrameTime, stage->avgRenderTime, stage->avgFrameTime));

    stage->updateFrame();
    // auto hit = stage->raytest({{0, 0, 0}, {1, 0, 0}});
    // qDebug()<<hit.actor;
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
    if(evt->key() == Qt::Key_Space){
        jumpFlag = true;
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
    // Ray ttfa = stage->pixelToRay(evt->pos().x(), evt->pos().y());
    // Raytest::intersectCallCnt = 0;
    // SceneRayHit hit = stage->raytest(ttfa);
    // qDebug()<<Raytest::intersectCallCnt;
    // if(hit.actor != nullptr){
    //     qDebug()<<hit.actor->meshID<<hit.triangleID<<hit.dis;
    //     hit.actor->mesh.triangles[hit.triangleID].shaderConfig |= ShaderConfig::WireframeOnly;
    // }
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
