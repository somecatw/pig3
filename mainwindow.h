#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QImage>
#include <QTimer>
#include "structures.h"

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

private:
    Ui::MainWindow *ui;
    int t;
    QImage *img;
    QLabel *label;
    QTimer *timer;
    Mesh ttfa;
};
#endif // MAINWINDOW_H
