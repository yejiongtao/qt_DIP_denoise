#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include "image_widget.h"

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = 0);
    ~Widget();

protected slots:
    void onLoadImage();
    void on5MedianFiltering();
    void on3MedianFiltering();
    void on7MedianFiltering();

signals:
    void launchMedianFiltering(int);

private:
    QVBoxLayout *m_layout;
    QVBoxLayout *m_btnLayout1;
    QVBoxLayout *m_btnLayout2;
    QHBoxLayout *m_menuLayout;
    ImageWidget *m_imageWidget;
    QPushButton *m_btn3MedianFiltering;
    QPushButton *m_btn5MedianFiltering;
    QPushButton *m_btn7MedianFiltering;
    QPushButton *m_btnAdaptiveMedianFiltering;
    QPushButton *m_btnSave;
    QPushButton *m_btnSaveAs;
    QPushButton *m_btnRestore;
};

#endif // WIDGET_H
