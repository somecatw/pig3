#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QImage>
#include <QTimer>
#include "stage3d.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void updateFrame();

protected:
    void keyPressEvent(QKeyEvent *evt) override;
    void keyReleaseEvent(QKeyEvent *evt) override;
    void mousePressEvent(QMouseEvent *evt) override;
    void mouseReleaseEvent(QMouseEvent *evt) override;
    void mouseMoveEvent(QMouseEvent *evt) override;

private:
    Ui::MainWindow *ui;
    int t;
    QLabel *fpsLabel;
    QTimer *timer;

    Stage3D *stage;
    Camera *camera;

    // 迫真 wasd 实现
    Transform camTrans;
    bool wasdFlags[4];
    bool dragFlag;
    bool jumpFlag;
    QPoint startPoint;
    int xacc, yacc;
};
#endif // MAINWINDOW_H
