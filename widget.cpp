#include "widget.h"
#include "global_defs.h"
#include <QPushButton>
#include <QString>
#include <QFileDialog>
#include <QDebug>

Widget::Widget(QWidget *parent)
    : QWidget(parent),m_imageWidget(0)
{
    QPushButton *btnLoadImage=new QPushButton("载入图片");
    connect(btnLoadImage,SIGNAL(clicked(bool)),this,SLOT(onLoadImage()));

    m_layout=new QVBoxLayout(this);
    m_menuLayout=new QHBoxLayout();
    m_btnLayout1=new QVBoxLayout();
    m_btnLayout2=new QVBoxLayout();

    m_layout->addLayout(m_menuLayout);
    m_menuLayout->addStretch(1);
    m_menuLayout->addLayout(m_btnLayout1);
    m_btnLayout1->addWidget(btnLoadImage,1);
    m_menuLayout->addStretch(1);

    m_btn3MedianFiltering=new QPushButton("3x3 Median Filter");
    m_btn3MedianFiltering->setEnabled(false);
    m_menuLayout->addLayout(m_btnLayout2);
    m_menuLayout->addStretch(1);
    m_btnLayout2->addWidget(m_btn3MedianFiltering);

    m_btn5MedianFiltering=new QPushButton("5x5 Median Filter");
    m_btn5MedianFiltering->setEnabled(false);
    m_btnLayout2->addWidget(m_btn5MedianFiltering);

    m_btn7MedianFiltering=new QPushButton("7x7 Median Filter");
    m_btn7MedianFiltering->setEnabled(false);
    m_btnLayout2->addWidget(m_btn7MedianFiltering);

    m_btnAdaptiveMedianFiltering=new QPushButton("Apaptive Median Filter");
    m_btnAdaptiveMedianFiltering->setEnabled(false);
    m_btnLayout2->addWidget(m_btnAdaptiveMedianFiltering);

    m_btnSave=new QPushButton("保存");
    m_btnSave->setEnabled(false);
    m_btnLayout1->addWidget(m_btnSave);

    m_btnSaveAs=new QPushButton("另存为");
    m_btnSaveAs->setEnabled(false);
    m_btnLayout1->addWidget(m_btnSaveAs);

    m_btnRestore=new QPushButton("恢复");
    m_btnRestore->setEnabled(false);
    m_btnLayout1->addWidget(m_btnRestore);
}

Widget::~Widget()
{
}

void Widget::onLoadImage()
{
    QString fileName=QFileDialog::getOpenFileName(this,"选择位图",QDir::currentPath(),"bitmaps(*.bmp)");

    if(!fileName.isEmpty())     //防止点取消的时候出错
    {
        if(m_imageWidget!=0)    //若之前已经载入过了，则杀掉之前的
            m_imageWidget->deleteLater();
        try
        {
            m_imageWidget=new ImageWidget(fileName);
            m_layout->addWidget(m_imageWidget);

            m_btn3MedianFiltering->setEnabled(true);
            m_btn5MedianFiltering->setEnabled(true);
            m_btn7MedianFiltering->setEnabled(true);
            m_btnAdaptiveMedianFiltering->setEnabled(true);
            m_btnSave->setEnabled(true);
            m_btnSaveAs->setEnabled(true);
            m_btnRestore->setEnabled(true);

            connect(m_btn3MedianFiltering,SIGNAL(clicked(bool)),this,SLOT(on3MedianFiltering()));
            connect(m_btn5MedianFiltering,SIGNAL(clicked(bool)),this,SLOT(on5MedianFiltering()));
            connect(m_btn7MedianFiltering,SIGNAL(clicked(bool)),this,SLOT(on7MedianFiltering()));
            connect(this,SIGNAL(launchMedianFiltering(int)),m_imageWidget,SLOT(onMedianFiltering(int)));
            connect(m_btnAdaptiveMedianFiltering,SIGNAL(clicked(bool)),m_imageWidget,SLOT(onAdaptiveMedianFiltering()));
            connect(m_btnSave,SIGNAL(clicked(bool)),m_imageWidget,SLOT(onSave()));
            connect(m_btnSaveAs,SIGNAL(clicked(bool)),m_imageWidget,SLOT(onSaveAs()));
            connect(m_btnRestore,SIGNAL(clicked(bool)),m_imageWidget,SLOT(onRestore()));
        }
        catch(int e)
        {
            if(e==FORMAT_ERROR)
            {
                m_btn3MedianFiltering->setEnabled(false);
                m_btn5MedianFiltering->setEnabled(false);
                m_btn7MedianFiltering->setEnabled(false);
                m_btnAdaptiveMedianFiltering->setEnabled(false);
                m_btnSave->setEnabled(false);
                m_btnSaveAs->setEnabled(false);
                m_btnRestore->setEnabled(false);
            }
        }
    }
}

void Widget::on3MedianFiltering()
{
    emit launchMedianFiltering(3);
}

void Widget::on5MedianFiltering()
{
    emit launchMedianFiltering(5);
}

void Widget::on7MedianFiltering()
{
    emit launchMedianFiltering(7);
}
