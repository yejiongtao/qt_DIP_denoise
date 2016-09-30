#include "image_widget.h"
#include "global_defs.h"
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QFileDialog>
#include <QDebug>

ImageWidget::ImageWidget(QString fileName, QWidget *parent)
    : QWidget(parent),m_fileName(fileName),m_isDirty(false),m_maxFilterSize(7),
      m_offBitsPos(10),m_widthPos(18),m_heightPos(22),
      m_bitCountPos(28),m_palettePos(54),m_paddingBits(0)
{
    m_file=new QFile(m_fileName);
    m_file->open(QFile::ReadOnly);         //注意要open
    m_fileSize=m_file->size();
    m_fileContent=new unsigned char[m_fileSize];
    m_fileBackup=new unsigned char[m_fileSize];

    m_file->read((char*)m_fileContent,m_fileSize);
    memcpy(m_fileBackup,m_fileContent,m_fileSize);

    if(m_fileContent[0]!=0x42 || m_fileContent[1]!=0x4D)            //bmp文件
    {
        QMessageBox::information(this,"error","This is not a bitmap.",QMessageBox::Ok);
        this->deleteLater();
        throw FORMAT_ERROR;
    }
    else if((m_bitCount=m_fileContent[m_bitCountPos])!=8 && m_bitCount!=24) //只允许8位和24位
    {
        QMessageBox::information(this,"error","This is not an 8-bitmap or a 24-bitmap.",QMessageBox::Ok);
        this->deleteLater();
        throw FORMAT_ERROR;
    }
    else
    {
        m_offBits=m_fileContent[m_offBitsPos]+m_fileContent[m_offBitsPos+1]*256
                +m_fileContent[m_offBitsPos+2]*256*256
                +m_fileContent[m_offBitsPos+3]*256*256*256;
        m_width=m_fileContent[m_widthPos]+m_fileContent[m_widthPos+1]*256
                +m_fileContent[m_widthPos+2]*256*256
                +m_fileContent[m_widthPos+3]*256*256*256;
        m_height=m_fileContent[m_heightPos]+m_fileContent[m_heightPos+1]*256
                +m_fileContent[m_heightPos+2]*256*256
                +m_fileContent[m_heightPos+3]*256*256*256;
        if(m_width%4!=0)
            m_paddingBits= 4 - m_width%4;
    }
}

ImageWidget::~ImageWidget()
{

}

void ImageWidget::paintEvent(QPaintEvent *e)
{
    QPainter painter(this);     //注意这个this，一定要的！
    QPen pen;

    if(m_bitCount==8)       //8位，256色，即灰度，有调色板。注意调色板每四字节表示一种颜色，因为有一字节是保留字节
    {
        if(m_height>0)          //高度>0，则图片信息是从最后一行开始储存的
        {
            int currentPos=m_offBits;
            for(int heightLoop=m_height-1;heightLoop>=0;heightLoop--)
            {
                for(int widthLoop=0;widthLoop!=m_width;widthLoop++)
                {
                    unsigned char b=m_fileContent[m_palettePos+4*m_fileContent[currentPos]];    //注意顺序！小头！！
                    unsigned char g=m_fileContent[m_palettePos+4*m_fileContent[currentPos]+1];
                    unsigned char r=m_fileContent[m_palettePos+4*m_fileContent[currentPos]+2];

                    currentPos++;
                    pen.setColor(QColor(r,g,b));
                    painter.setPen(pen);            //每次换颜色之后都要重新setPen
                    painter.drawPoint(widthLoop,heightLoop);
                }
                currentPos+=m_paddingBits;          //填充的东西要空过去！！！
            }
        }
        else                //m_height<0，图片信息从第一行开始储存
        {
            int currentPos=m_offBits;
            for(int heightLoop=0;heightLoop!=-m_height;heightLoop++)
            {
                for(int widthLoop=0;widthLoop!=m_width;widthLoop++)
                {
                    unsigned char b=m_fileContent[m_palettePos+4*m_fileContent[currentPos]];
                    unsigned char g=m_fileContent[m_palettePos+4*m_fileContent[currentPos]+1];
                    unsigned char r=m_fileContent[m_palettePos+4*m_fileContent[currentPos]+2];
                    currentPos++;
                    pen.setColor(QColor(r,g,b));
                    painter.setPen(pen);
                    painter.drawPoint(widthLoop,heightLoop);
                }
                currentPos+=m_paddingBits;
            }
        }
    }
    else if(m_bitCount==24)     //24位，真彩色
    {
        if(m_height>0)
        {
            int currentPos=m_offBits;
            for(int heightLoop=m_height-1;heightLoop>=0;heightLoop--)
            {
                for(int widthLoop=0;widthLoop!=m_width;widthLoop++)
                {
                    unsigned char b=m_fileContent[currentPos];      //注意顺序！！！小头啊小头
                    unsigned char g=m_fileContent[currentPos+1];
                    unsigned char r=m_fileContent[currentPos+2];
                    currentPos+=3;
                    pen.setColor(QColor(r,g,b));
                    painter.setPen(pen);
                    painter.drawPoint(widthLoop,heightLoop);
                }
                currentPos+=m_paddingBits*3;
            }
        }
        else
        {
            int currentPos=m_offBits;
            for(int heightLoop=0;heightLoop!=-m_height;heightLoop++)
            {
                for(int widthLoop=0;widthLoop!=m_width;widthLoop++)
                {
                    unsigned char b=m_fileContent[currentPos];
                    unsigned char g=m_fileContent[currentPos+1];
                    unsigned char r=m_fileContent[currentPos+2];
                    currentPos+=3;
                    pen.setColor(QColor(r,g,b));
                    painter.setPen(pen);
                    painter.drawPoint(widthLoop,heightLoop);
                }
                currentPos+=m_paddingBits*3;
            }
        }
    }
    e->accept();
}

void ImageWidget::onMedianFiltering(int level)
{
    m_isDirty=true;

    int height=m_height;
    if(height<0)            //无论是从第一行开始储存还是从最后一行开始储存，都按储存顺序进行处理就可以
        height=-height;

    int currentPos=m_offBits;
    unsigned char r=0,g=0,b=0;

    for(int heightLoop=0;heightLoop<height;heightLoop++)
    {
        for(int widthLoop=0;widthLoop!=m_width;widthLoop++)
        {
            //图片的边界部分，整个的mask并不能fit in；但是又不能直接不管，成为盲区。所以就得分类讨论，在边界的地方apply mask的一部分
            if(level==3)
            {
                if(heightLoop==0)   //第一行（这里的第一行不一定是图像的最上面，而是内存中储存在最前面的那一行）
                {
                    if(widthLoop==0)                        //第一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,8,9};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-1)           //最后一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,6,7,8};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else                                    //中间的点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,6,7,8,9};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                }
                else if(heightLoop==height-1)               //最后一行
                {
                    if(widthLoop==0)                        //第一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,3,4};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-1)               //最后一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,4,5,6};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else                //中间的点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,3,4,5,6};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                }
                else                                        //除了第一行和最后一行
                {
                    if(widthLoop==0)                        //第一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,3,4,8,9};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-1)           //最后一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,4,5,6,7,8};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else                                    //中间的点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,3,4,5,6,7,8,9};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                }
            }
            else if(level==5)
            {
                if(heightLoop==0)   //第一行（这里的第一行不一定是图像的最上面，而是内存中储存在最前面的那一行）
                {
                    if(widthLoop==0)                        //第一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,8,9,10,22,23,24,25};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==1)               //第二点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,6,7,8,9,10,21,22,23,24,25};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-2)       //倒数第二点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={18,6,1,2,19,7,8,9,20,21,22,23};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-1)           //最后一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={18,6,1,19,7,8,20,21,22};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else                                    //中间的点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={18,6,1,2,10,19,7,8,9,25,20,21,22,23,24};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                }
                else if(heightLoop==1)  //第二行
                {
                    if(widthLoop==0)                        //第一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,8,9,10,22,23,24,25,4,3,11};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==1)               //第二点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,6,7,8,9,10,21,22,23,24,25,5,4,3,11};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-2)       //倒数第二点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={18,6,1,2,19,7,8,9,20,21,22,23,17,5,4,3};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-1)           //最后一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={18,6,1,19,7,8,20,21,22,17,5,4};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else                                    //中间的点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={18,6,1,2,10,19,7,8,9,25,20,21,22,23,24,17,5,4,3,11};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                }
                else if(heightLoop==height-2)   //倒数第二行
                {
                    if(widthLoop==0)                        //第一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={14,13,12,11,4,3,1,2,10,8,9,25};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==1)               //第二点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={15,14,13,12,5,4,3,11,6,1,2,10,7,8,9,25};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-2)       //倒数第二点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={16,15,14,13,17,5,4,3,18,6,1,2,19,7,8,9};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-1)           //最后一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={16,15,14,17,5,4,18,6,1,19,7,8};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else                                    //中间的点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={18,6,1,2,10,19,7,8,9,25,17,5,4,3,11,16,15,14,13,12};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                }
                else if(heightLoop==height-1)               //最后一行
                {
                    if(widthLoop==0)                        //第一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={14,13,12,11,4,3,1,2,10};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==1)               //第二点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={15,14,13,12,5,4,3,11,6,1,2,10};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-2)       //倒数第二点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={16,15,14,13,17,5,4,3,18,6,1,2};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-1)           //最后一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={16,15,14,17,5,4,18,6,1};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else                                    //中间的点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={18,6,1,2,10,17,5,4,3,11,16,15,14,13,12};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                }
                else                                        //中间的行
                {
                    if(widthLoop==0)                        //第一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={14,13,12,4,3,11,1,2,10,8,9,25,22,23,24};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==1)               //第二点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={15,14,13,12,5,4,3,11,6,1,2,10,7,8,9,25,21,22,23,24};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-2)       //倒数第二点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={16,15,14,13,17,5,4,3,18,6,1,2,19,7,8,9,20,21,22,23};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-1)           //最后一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={16,15,14,17,5,4,18,6,1,19,7,8,20,21,22};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else                                    //中间的点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                }
            }
            else if(level==7)
            {
                if(heightLoop==0)   //第一行（这里的第一行不一定是图像的最上面，而是内存中储存在最前面的那一行）
                {
                    if(widthLoop==0)                        //第一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,10,26,8,9,25,49,22,23,24,48,44,45,46,47};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==1)               //第二点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,10,26,8,9,25,49,22,23,24,48,44,45,46,47, 6,7,21,43};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==2)               //第三点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,10,26,8,9,25,49,22,23,24,48,44,45,46,47, 6,7,21,43, 18,19,20,42};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-3)       //倒数第三点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={38,18,6,1,39,19,7,8,40,20,21,22,41,42,43,44, 2,9,23,45, 10,25,24,46};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-2)       //倒数第二点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={38,18,6,1,39,19,7,8,40,20,21,22,41,42,43,44, 2,9,23,45};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-1)           //最后一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={38,18,6,1,39,19,7,8,40,20,21,22,41,42,43,44};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else                                    //中间的点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,10,26,8,9,25,49,22,23,24,48,44,45,46,47, 6,7,21,43, 18,19,20,42, 38,39,40,41};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                }
                else if(heightLoop==1)  //第二行
                {
                    if(widthLoop==0)                        //第一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,10,26,8,9,25,49,22,23,24,48,44,45,46,47, 4,3,11,27};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==1)               //第二点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,10,26,8,9,25,49,22,23,24,48,44,45,46,47, 6,7,21,43, 5,4,3,11,27};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==2)               //第三点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,10,26,8,9,25,49,22,23,24,48,44,45,46,47, 6,7,21,43, 18,19,20,42, 17,5,4,3,11,27};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-3)       //倒数第三点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={38,18,6,1,39,19,7,8,40,20,21,22,41,42,43,44, 2,9,23,45, 10,25,24,46, 37,17,5,4,3,11};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-2)       //倒数第二点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={38,18,6,1,39,19,7,8,40,20,21,22,41,42,43,44, 2,9,23,45, 37,17,5,4,3};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-1)           //最后一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={38,18,6,1,39,19,7,8,40,20,21,22,41,42,43,44, 37,17,5,4};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else                                    //中间的点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,10,26,8,9,25,49,22,23,24,48,44,45,46,47, 6,7,21,43, 18,19,20,42, 38,39,40,41, 37,17,5,4,3,11,27};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                }
                else if(heightLoop==2)  //第三行
                {
                    if(widthLoop==0)                        //第一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,10,26,8,9,25,49,22,23,24,48,44,45,46,47, 4,3,11,27, 14,13,12,28};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==1)               //第二点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,10,26,8,9,25,49,22,23,24,48,44,45,46,47, 6,7,21,43, 5,4,3,11,27, 15,14,13,12,28};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==2)               //第三点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,10,26,8,9,25,49,22,23,24,48,44,45,46,47, 6,7,21,43, 18,19,20,42, 17,5,4,3,11,27, 16,15,14,13,12,28};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-3)       //倒数第三点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={38,18,6,1,39,19,7,8,40,20,21,22,41,42,43,44, 2,9,23,45, 10,25,24,46, 37,17,5,4,3,11, 36,16,15,14,13,12};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-2)       //倒数第二点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={38,18,6,1,39,19,7,8,40,20,21,22,41,42,43,44, 2,9,23,45, 37,17,5,4,3, 36,16,15,14,13};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-1)           //最后一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={38,18,6,1,39,19,7,8,40,20,21,22,41,42,43,44, 37,17,5,4, 36,16,15,14};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else                                    //中间的点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,10,26,8,9,25,49,22,23,24,48,44,45,46,47, 6,7,21,43, 18,19,20,42, 38,39,40,41, 37,17,5,4,3,11,27, 36,16,15,14,13,12,28};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                }
                else if(heightLoop==height-3)   //倒数第三行
                {
                    if(widthLoop==0)                        //第一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 8,9,25,49, 22,23,24,48};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==1)               //第二点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 33,15,5,6, 7,8,9,25,49, 21,22,23,24,48};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==2)               //第三点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 33,15,5,6, 34,16,17,18, 19,7,8,9,25,49, 20,21,22,23,24,48};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-3)       //倒数第三点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={35,34,33,32,36,16,15,14,37,17,5,4,38,18,6,1, 31,13,3,2, 30,12,11,10, 39,19,7,8,9,25, 40,20,21,22,23,24};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-2)       //倒数第二点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={35,34,33,32,36,16,15,14,37,17,5,4,38,18,6,1, 31,13,3,2, 39,19,7,8,9, 40,20,21,22,23};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-1)           //最后一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={35,34,33,32,36,16,15,14,37,17,5,4,38,18,6,1, 39,19,7,8, 40,20,21,22};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else                                    //中间的点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 33,15,5,6, 34,16,17,18, 35,36,37,38, 39,19,7,8,9,25,49, 40,20,21,22,23,24,48};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                }
                else if(heightLoop==height-2)   //倒数第二行
                {
                    if(widthLoop==0)                        //第一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 8,9,25,49};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==1)               //第二点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 33,15,5,6, 7,8,9,25,49};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==2)               //第三点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 33,15,5,6, 34,16,17,18, 19,7,8,9,25,49};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-3)       //倒数第三点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={35,34,33,32,36,16,15,14,37,17,5,4,38,18,6,1, 31,13,3,2, 30,12,11,10, 39,19,7,8,9,25};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-2)       //倒数第二点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={35,34,33,32,36,16,15,14,37,17,5,4,38,18,6,1, 31,13,3,2, 39,19,7,8,9};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-1)           //最后一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={35,34,33,32,36,16,15,14,37,17,5,4,38,18,6,1, 39,19,7,8};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else                                    //中间的点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 33,15,5,6, 34,16,17,18, 35,36,37,38, 39,19,7,8,9,25,49};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                }
                else if(heightLoop==height-1)               //最后一行
                {
                    if(widthLoop==0)                        //第一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==1)               //第二点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 33,15,5,6};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==2)               //第三点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 33,15,5,6, 34,16,17,18};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-3)       //倒数第三点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={35,34,33,32,36,16,15,14,37,17,5,4,38,18,6,1, 31,13,3,2, 30,12,11,10};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-2)       //倒数第二点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={35,34,33,32,36,16,15,14,37,17,5,4,38,18,6,1, 31,13,3,2};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-1)           //最后一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={35,34,33,32,36,16,15,14,37,17,5,4,38,18,6,1};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else                                    //中间的点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 33,15,5,6, 34,16,17,18, 35,36,37,38};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                }
                else                                        //中间的行
                {
                    if(widthLoop==0)                        //第一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 8,9,25,49, 22,23,24,48, 44,45,46,47};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==1)               //第二点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 33,15,5,6, 7,8,9,25,49, 21,22,23,24,48, 43,44,45,46,47};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==2)               //第三点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 33,15,5,6, 34,16,17,18, 19,7,8,9,25,49, 20,21,22,23,24,48, 42,43,44,45,46,47};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-3)       //倒数第三点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={35,34,33,32,36,16,15,14,37,17,5,4,38,18,6,1, 31,13,3,2, 30,12,11,10, 39,19,7,8,9,25, 40,20,21,22,23,24, 41,42,43,44,45,46};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-2)       //倒数第二点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={35,34,33,32,36,16,15,14,37,17,5,4,38,18,6,1, 31,13,3,2, 39,19,7,8,9, 40,20,21,22,23, 41,42,43,44,45};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else if(widthLoop==m_width-1)           //最后一点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={35,34,33,32,36,16,15,14,37,17,5,4,38,18,6,1, 39,19,7,8, 40,20,21,22, 41,42,43,44};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                    else                                    //中间的点
                    {
                        unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 33,15,5,6, 34,16,17,18, 35,36,37,38, 39,19,7,8,9,25,49, 40,20,21,22,23,24,48, 41,42,43,44,45,46,47};
                        this->GetMedianInAMask(locations,currentPos,&r,&g,&b);
                    }
                }
            }

            //8位的图是有调色板的，要找到颜色值对应的调色板，把调色板的编号存在内容里面
            if(m_bitCount==8)
            {
                for(int i=m_palettePos;i<m_offBits;i+=4)
                {
                    if(m_fileContent[i+2]==r && m_fileContent[i+1]==g && m_fileContent[i]==b)
                    {
                        m_fileContent[currentPos]=(unsigned char)((i-m_palettePos)/4);
                    }
                }
                currentPos++;
            }
            //24位的没有调色板，直接把颜色值存在内容里面
            else if(m_bitCount==24)
            {
                m_fileContent[currentPos+2]=r;
                m_fileContent[currentPos+1]=g;
                m_fileContent[currentPos]=b;

                currentPos+=3;
            }
        }
        if(m_bitCount==8)
            currentPos+=(m_paddingBits);          //填充的东西要空过去
        else if(m_bitCount==24)
            currentPos+=m_paddingBits*3;          //填充的东西要空过去
    }

    update();
}

void ImageWidget::onAdaptiveMedianFiltering()
{
    m_isDirty=true;

    int height=m_height;
    if(height<0)            //无论是从第一行开始储存还是从最后一行开始储存，都按储存顺序进行处理就可以
        height=-height;

    int currentPos=m_offBits;
    unsigned char r=0,g=0,b=0;

    for(int heightLoop=0;heightLoop<height;heightLoop++)
    {
        for(int widthLoop=0;widthLoop!=m_width;widthLoop++)
        {
            int currentSize=3;  //当前的mask大小
            bool ifMedianInRange=false,ifThisInRange=false;

            while(1)
            {
                //根据currentSize分成几类
                if(currentSize==3)
                {
                    if(heightLoop==0)   //第一行（这里的第一行不一定是图像的最上面，而是内存中储存在最前面的那一行）
                    {
                        if(widthLoop==0)                        //第一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,8,9};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-1)           //最后一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,6,7,8};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else                                    //中间的点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,6,7,8,9};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                    }
                    else if(heightLoop==height-1)               //最后一行
                    {
                        if(widthLoop==0)                        //第一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,3,4};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-1)               //最后一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,4,5,6};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else                //中间的点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,3,4,5,6};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                    }
                    else                                        //除了第一行和最后一行
                    {
                        if(widthLoop==0)                        //第一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,3,4,8,9};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-1)           //最后一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,4,5,6,7,8};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else                                    //中间的点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,3,4,5,6,7,8,9};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                    }
                }
                else if(currentSize==5)
                {
                    if(heightLoop==0)   //第一行（这里的第一行不一定是图像的最上面，而是内存中储存在最前面的那一行）
                    {
                        if(widthLoop==0)                        //第一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,8,9,10,22,23,24,25};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==1)               //第二点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,6,7,8,9,10,21,22,23,24,25};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-2)       //倒数第二点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={18,6,1,2,19,7,8,9,20,21,22,23};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-1)           //最后一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={18,6,1,19,7,8,20,21,22};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else                                    //中间的点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={18,6,1,2,10,19,7,8,9,25,20,21,22,23,24};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                    }
                    else if(heightLoop==1)  //第二行
                    {
                        if(widthLoop==0)                        //第一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,8,9,10,22,23,24,25,4,3,11};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==1)               //第二点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,6,7,8,9,10,21,22,23,24,25,5,4,3,11};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-2)       //倒数第二点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={18,6,1,2,19,7,8,9,20,21,22,23,17,5,4,3};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-1)           //最后一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={18,6,1,19,7,8,20,21,22,17,5,4};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else                                    //中间的点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={18,6,1,2,10,19,7,8,9,25,20,21,22,23,24,17,5,4,3,11};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                    }
                    else if(heightLoop==height-2)   //倒数第二行
                    {
                        if(widthLoop==0)                        //第一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={14,13,12,11,4,3,1,2,10,8,9,25};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==1)               //第二点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={15,14,13,12,5,4,3,11,6,1,2,10,7,8,9,25};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-2)       //倒数第二点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={16,15,14,13,17,5,4,3,18,6,1,2,19,7,8,9};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-1)           //最后一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={16,15,14,17,5,4,18,6,1,19,7,8};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else                                    //中间的点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={18,6,1,2,10,19,7,8,9,25,17,5,4,3,11,16,15,14,13,12};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                    }
                    else if(heightLoop==height-1)               //最后一行
                    {
                        if(widthLoop==0)                        //第一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={14,13,12,11,4,3,1,2,10};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==1)               //第二点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={15,14,13,12,5,4,3,11,6,1,2,10};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-2)       //倒数第二点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={16,15,14,13,17,5,4,3,18,6,1,2};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-1)           //最后一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={16,15,14,17,5,4,18,6,1};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else                                    //中间的点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={18,6,1,2,10,17,5,4,3,11,16,15,14,13,12};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                    }
                    else                                        //中间的行
                    {
                        if(widthLoop==0)                        //第一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={14,13,12,4,3,11,1,2,10,8,9,25,22,23,24};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==1)               //第二点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={15,14,13,12,5,4,3,11,6,1,2,10,7,8,9,25,21,22,23,24};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-2)       //倒数第二点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={16,15,14,13,17,5,4,3,18,6,1,2,19,7,8,9,20,21,22,23};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-1)           //最后一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={16,15,14,17,5,4,18,6,1,19,7,8,20,21,22};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else                                    //中间的点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                    }
                }
                else if(currentSize==7)
                {
                    if(heightLoop==0)   //第一行（这里的第一行不一定是图像的最上面，而是内存中储存在最前面的那一行）
                    {
                        if(widthLoop==0)                        //第一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,10,26,8,9,25,49,22,23,24,48,44,45,46,47};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==1)               //第二点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,10,26,8,9,25,49,22,23,24,48,44,45,46,47, 6,7,21,43};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==2)               //第三点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,10,26,8,9,25,49,22,23,24,48,44,45,46,47, 6,7,21,43, 18,19,20,42};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-3)       //倒数第三点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={38,18,6,1,39,19,7,8,40,20,21,22,41,42,43,44, 2,9,23,45, 10,25,24,46};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-2)       //倒数第二点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={38,18,6,1,39,19,7,8,40,20,21,22,41,42,43,44, 2,9,23,45};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-1)           //最后一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={38,18,6,1,39,19,7,8,40,20,21,22,41,42,43,44};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else                                    //中间的点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,10,26,8,9,25,49,22,23,24,48,44,45,46,47, 6,7,21,43, 18,19,20,42, 38,39,40,41};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                    }
                    else if(heightLoop==1)  //第二行
                    {
                        if(widthLoop==0)                        //第一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,10,26,8,9,25,49,22,23,24,48,44,45,46,47, 4,3,11,27};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==1)               //第二点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,10,26,8,9,25,49,22,23,24,48,44,45,46,47, 6,7,21,43, 5,4,3,11,27};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==2)               //第三点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,10,26,8,9,25,49,22,23,24,48,44,45,46,47, 6,7,21,43, 18,19,20,42, 17,5,4,3,11,27};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-3)       //倒数第三点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={38,18,6,1,39,19,7,8,40,20,21,22,41,42,43,44, 2,9,23,45, 10,25,24,46, 37,17,5,4,3,11};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-2)       //倒数第二点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={38,18,6,1,39,19,7,8,40,20,21,22,41,42,43,44, 2,9,23,45, 37,17,5,4,3};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-1)           //最后一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={38,18,6,1,39,19,7,8,40,20,21,22,41,42,43,44, 37,17,5,4};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else                                    //中间的点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,10,26,8,9,25,49,22,23,24,48,44,45,46,47, 6,7,21,43, 18,19,20,42, 38,39,40,41, 37,17,5,4,3,11,27};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                    }
                    else if(heightLoop==2)  //第三行
                    {
                        if(widthLoop==0)                        //第一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,10,26,8,9,25,49,22,23,24,48,44,45,46,47, 4,3,11,27, 14,13,12,28};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==1)               //第二点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,10,26,8,9,25,49,22,23,24,48,44,45,46,47, 6,7,21,43, 5,4,3,11,27, 15,14,13,12,28};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==2)               //第三点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,10,26,8,9,25,49,22,23,24,48,44,45,46,47, 6,7,21,43, 18,19,20,42, 17,5,4,3,11,27, 16,15,14,13,12,28};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-3)       //倒数第三点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={38,18,6,1,39,19,7,8,40,20,21,22,41,42,43,44, 2,9,23,45, 10,25,24,46, 37,17,5,4,3,11, 36,16,15,14,13,12};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-2)       //倒数第二点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={38,18,6,1,39,19,7,8,40,20,21,22,41,42,43,44, 2,9,23,45, 37,17,5,4,3, 36,16,15,14,13};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-1)           //最后一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={38,18,6,1,39,19,7,8,40,20,21,22,41,42,43,44, 37,17,5,4, 36,16,15,14};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else                                    //中间的点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={1,2,10,26,8,9,25,49,22,23,24,48,44,45,46,47, 6,7,21,43, 18,19,20,42, 38,39,40,41, 37,17,5,4,3,11,27, 36,16,15,14,13,12,28};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                    }
                    else if(heightLoop==height-3)   //倒数第三行
                    {
                        if(widthLoop==0)                        //第一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 8,9,25,49, 22,23,24,48};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==1)               //第二点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 33,15,5,6, 7,8,9,25,49, 21,22,23,24,48};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==2)               //第三点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 33,15,5,6, 34,16,17,18, 19,7,8,9,25,49, 20,21,22,23,24,48};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-3)       //倒数第三点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={35,34,33,32,36,16,15,14,37,17,5,4,38,18,6,1, 31,13,3,2, 30,12,11,10, 39,19,7,8,9,25, 40,20,21,22,23,24};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-2)       //倒数第二点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={35,34,33,32,36,16,15,14,37,17,5,4,38,18,6,1, 31,13,3,2, 39,19,7,8,9, 40,20,21,22,23};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-1)           //最后一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={35,34,33,32,36,16,15,14,37,17,5,4,38,18,6,1, 39,19,7,8, 40,20,21,22};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else                                    //中间的点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 33,15,5,6, 34,16,17,18, 35,36,37,38, 39,19,7,8,9,25,49, 40,20,21,22,23,24,48};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                    }
                    else if(heightLoop==height-2)   //倒数第二行
                    {
                        if(widthLoop==0)                        //第一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 8,9,25,49};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==1)               //第二点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 33,15,5,6, 7,8,9,25,49};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==2)               //第三点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 33,15,5,6, 34,16,17,18, 19,7,8,9,25,49};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-3)       //倒数第三点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={35,34,33,32,36,16,15,14,37,17,5,4,38,18,6,1, 31,13,3,2, 30,12,11,10, 39,19,7,8,9,25};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-2)       //倒数第二点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={35,34,33,32,36,16,15,14,37,17,5,4,38,18,6,1, 31,13,3,2, 39,19,7,8,9};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-1)           //最后一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={35,34,33,32,36,16,15,14,37,17,5,4,38,18,6,1, 39,19,7,8};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else                                    //中间的点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 33,15,5,6, 34,16,17,18, 35,36,37,38, 39,19,7,8,9,25,49};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                    }
                    else if(heightLoop==height-1)               //最后一行
                    {
                        if(widthLoop==0)                        //第一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==1)               //第二点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 33,15,5,6};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==2)               //第三点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 33,15,5,6, 34,16,17,18};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-3)       //倒数第三点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={35,34,33,32,36,16,15,14,37,17,5,4,38,18,6,1, 31,13,3,2, 30,12,11,10};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-2)       //倒数第二点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={35,34,33,32,36,16,15,14,37,17,5,4,38,18,6,1, 31,13,3,2};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-1)           //最后一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={35,34,33,32,36,16,15,14,37,17,5,4,38,18,6,1};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else                                    //中间的点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 33,15,5,6, 34,16,17,18, 35,36,37,38};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                    }
                    else                                        //中间的行
                    {
                        if(widthLoop==0)                        //第一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 8,9,25,49, 22,23,24,48, 44,45,46,47};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==1)               //第二点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 33,15,5,6, 7,8,9,25,49, 21,22,23,24,48, 43,44,45,46,47};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==2)               //第三点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 33,15,5,6, 34,16,17,18, 19,7,8,9,25,49, 20,21,22,23,24,48, 42,43,44,45,46,47};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-3)       //倒数第三点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={35,34,33,32,36,16,15,14,37,17,5,4,38,18,6,1, 31,13,3,2, 30,12,11,10, 39,19,7,8,9,25, 40,20,21,22,23,24, 41,42,43,44,45,46};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-2)       //倒数第二点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={35,34,33,32,36,16,15,14,37,17,5,4,38,18,6,1, 31,13,3,2, 39,19,7,8,9, 40,20,21,22,23, 41,42,43,44,45};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else if(widthLoop==m_width-1)           //最后一点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={35,34,33,32,36,16,15,14,37,17,5,4,38,18,6,1, 39,19,7,8, 40,20,21,22, 41,42,43,44};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                        else                                    //中间的点
                        {
                            unsigned char locations[m_maxFilterSize*m_maxFilterSize]={32,31,30,29,14,13,12,28,4,3,11,27,1,2,10,26, 33,15,5,6, 34,16,17,18, 35,36,37,38, 39,19,7,8,9,25,49, 40,20,21,22,23,24,48, 41,42,43,44,45,46,47};
                            this->GetMedianInAMask(locations,currentPos,&r,&g,&b,&ifMedianInRange,&ifThisInRange);
                        }
                    }
                }
                //目前支持的最大size是7x7

                //根据结果自适应判断改取什么颜色值
                if(ifMedianInRange)
                {
                    if(ifThisInRange)
                    {
                        if(m_bitCount==8)   //8位bitmap
                        {
                            r=m_fileContent[m_palettePos+4*m_fileContent[currentPos]+2];
                            g=m_fileContent[m_palettePos+4*m_fileContent[currentPos]+1];
                            b=m_fileContent[m_palettePos+4*m_fileContent[currentPos]];
                        }
                        else if(m_bitCount==24) //24位bitmap
                        {
                            r=m_fileContent[currentPos+2];
                            g=m_fileContent[currentPos+1];
                            b=m_fileContent[currentPos];
                        }
                    }
                    break;
                }
                else
                {
                    currentSize+=2;
                    if(currentSize>m_maxFilterSize)
                    {
                        if(m_bitCount==8)
                        {
                            r=m_fileContent[m_palettePos+4*m_fileContent[currentPos]+2];
                            g=m_fileContent[m_palettePos+4*m_fileContent[currentPos]+1];
                            b=m_fileContent[m_palettePos+4*m_fileContent[currentPos]];
                        }
                        else if(m_bitCount==24)
                        {
                            r=m_fileContent[currentPos+2];
                            g=m_fileContent[currentPos+1];
                            b=m_fileContent[currentPos];
                        }
                        break;
                    }
                }
            }

            if(m_bitCount==8)
            {
                for(int i=m_palettePos;i<m_offBits;i+=4)
                {
                    if(m_fileContent[i+2]==r && m_fileContent[i+1]==g && m_fileContent[i]==b)
                    {
                        m_fileContent[currentPos]=(unsigned char)((i-m_palettePos)/4);
                    }
                }
                currentPos++;
            }
            else if(m_bitCount==24)
            {
                m_fileContent[currentPos+2]=r;
                m_fileContent[currentPos+1]=g;
                m_fileContent[currentPos]=b;

                currentPos+=3;
            }

        }
        if(m_bitCount==8)
            currentPos+=(m_paddingBits);          //填充的东西要空过去
        else if(m_bitCount==24)
            currentPos+=m_paddingBits*3;          //填充的东西要空过去
    }
    update();
}

void ImageWidget::onSave()
{
    if(m_isDirty)
    {
        m_file->close();                    //要关掉当前文件，重新以WirteOnly的模式重新建一个，因为ReadWrite模式是append的
        m_file->deleteLater();
        m_file=new QFile(m_fileName);
        m_file->open(QFile::WriteOnly);
        m_file->write((char *)m_fileContent,m_fileSize);

        memcpy(m_fileBackup,m_fileContent,m_fileSize);  //保存以后“恢复”功能就以当前的图像为基准了
        m_isDirty=false;
    }
    QMessageBox::information(this,"Information","保存成功！");
}

void ImageWidget::onSaveAs()
{
    m_file->close();
    m_file->deleteLater();
                            //getSaveFileName作用也仅仅是范围一个文件名，与getOpenFileName的区别在于返回的可以是不存在的文件
    m_fileName=QFileDialog::getSaveFileName(this,"Save",QDir::currentPath(),"bitmaps(*.bmp)");
    m_file=new QFile(m_fileName);
    m_file->open(QFile::WriteOnly);
    m_file->write((char *)m_fileContent,m_fileSize);
    memcpy(m_fileBackup,m_fileContent,m_fileSize);

    QMessageBox::information(this,"Information","保存成功！");
    m_isDirty=false;
}

//返回中位数
unsigned char ImageWidget::FindMedian(unsigned char a[], int count)
{
    for(int i=0;i<count-1;i++)
    {
        for(int j=0;j<count-i-1;j++)
        {
            if(a[j]>a[j+1])
            {
                unsigned char b=a[j];
                a[j]=a[j+1];
                a[j+1]=b;
            }
        }
    }
    return a[count/2];
}

//给定一个存有位置的数组locations，位置范围是1 - m_maxFilterSize*m_maxFilterSize
//以3x3的mask为例，位置的顺序如下：（参数currentPos就是位置1在文件中的真实位置）
//  5 4 3
//  6 1 2
//  7 8 9
//5x5的mask：
//  16 15 14 13 12
//  17  5  4  3 11
//  18  6  1  2 10
//  19  7  8  9 25
//  20 21 22 23 24
//只要指定了位置，就可以读出所要的东西
//要求locations数组中所有非零的都位于0前面，数组大小为m_maxFilterSize*m_maxFilterSize
//返回这些点的r、g、b中位数，存在给的参数*r,*g,*b中
//返回两个bool值，分别表示：中位数是否小于最大数且大于最小数；中点的值是否小于最大数且大于最小数
//  分别储存在*ifMedianInRange,*ifThisInRange中
//目前支持最大的mask是7x7，要再大可以继续写
void ImageWidget::GetMedianInAMask(unsigned char locations[],int currentPos,
                                   unsigned char *r,unsigned char *g,unsigned char *b,
                                   bool *ifMedianInRange,bool *ifThisInRange)
{
    int count=0;    //记录总共有多少位是有数据的
    unsigned char bufferRed[m_maxFilterSize*m_maxFilterSize]={0};
    unsigned char bufferGreen[m_maxFilterSize*m_maxFilterSize]={0};
    unsigned char bufferBlue[m_maxFilterSize*m_maxFilterSize]={0};

    for(int i=0;locations[i]!=0 && i<m_maxFilterSize*m_maxFilterSize;i++,count++)
    {
        if(m_bitCount==8)
        {
            switch (locations[i])
            {
            case 1:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos]];
                break;
            case 2:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+1]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+1]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+1]];
                break;
            case 3:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-m_width-m_paddingBits+1]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-m_width-m_paddingBits+1]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-m_width-m_paddingBits+1]];
                break;
            case 4:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-m_width-m_paddingBits]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-m_width-m_paddingBits]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-m_width-m_paddingBits]];
                break;
            case 5:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-m_width-m_paddingBits-1]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-m_width-m_paddingBits-1]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-m_width-m_paddingBits-1]];
                break;
            case 6:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-1]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-1]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-1]];
                break;
            case 7:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+m_width+m_paddingBits-1]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+m_width+m_paddingBits-1]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+m_width+m_paddingBits-1]];
                break;
            case 8:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+m_width+m_paddingBits]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+m_width+m_paddingBits]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+m_width+m_paddingBits]];
                break;
            case 9:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+m_width+m_paddingBits+1]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+m_width+m_paddingBits+1]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+m_width+m_paddingBits+1]];
                break;
            case 10:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+2]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+2]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+2]];
                break;
            case 11:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-m_width-m_paddingBits+2]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-m_width-m_paddingBits+2]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-m_width-m_paddingBits+2]];
                break;
            case 12:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-2*m_width-2*m_paddingBits+2]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-2*m_width-2*m_paddingBits+2]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-2*m_width-2*m_paddingBits+2]];
                break;
            case 13:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-2*m_width-2*m_paddingBits+1]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-2*m_width-2*m_paddingBits+1]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-2*m_width-2*m_paddingBits+1]];
                break;
            case 14:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-2*m_width-2*m_paddingBits]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-2*m_width-2*m_paddingBits]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-2*m_width-2*m_paddingBits]];
                break;
            case 15:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-2*m_width-2*m_paddingBits-1]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-2*m_width-2*m_paddingBits-1]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-2*m_width-2*m_paddingBits-1]];
                break;
            case 16:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-2*m_width-2*m_paddingBits-2]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-2*m_width-2*m_paddingBits-2]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-2*m_width-2*m_paddingBits-2]];
                break;
            case 17:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-m_width-m_paddingBits-2]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-m_width-m_paddingBits-2]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-m_width-m_paddingBits-2]];
                break;
            case 18:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-2]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-2]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-2]];
                break;
            case 19:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+m_width+m_paddingBits-2]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+m_width+m_paddingBits-2]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+m_width+m_paddingBits-2]];
                break;
            case 20:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+2*m_width+2*m_paddingBits-2]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+2*m_width+2*m_paddingBits-2]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+2*m_width+2*m_paddingBits-2]];
                break;
            case 21:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+2*m_width+2*m_paddingBits-1]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+2*m_width+2*m_paddingBits-1]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+2*m_width+2*m_paddingBits-1]];
                break;
            case 22:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+2*m_width+2*m_paddingBits]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+2*m_width+2*m_paddingBits]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+2*m_width+2*m_paddingBits]];
                break;
            case 23:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+2*m_width+2*m_paddingBits+1]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+2*m_width+2*m_paddingBits+1]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+2*m_width+2*m_paddingBits+1]];
                break;
            case 24:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+2*m_width+2*m_paddingBits+2]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+2*m_width+2*m_paddingBits+2]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+2*m_width+2*m_paddingBits+2]];
                break;
            case 25:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+m_width+m_paddingBits+2]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+m_width+m_paddingBits+2]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+m_width+m_paddingBits+2]];
                break;
            case 26:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+3]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+3]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+3]];
                break;
            case 27:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-m_width-m_paddingBits+3]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-m_width-m_paddingBits+3]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-m_width-m_paddingBits+3]];
                break;
            case 28:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-2*m_width-2*m_paddingBits+3]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-2*m_width-2*m_paddingBits+3]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-2*m_width-2*m_paddingBits+3]];
                break;
            case 29:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-3*m_width-3*m_paddingBits+3]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-3*m_width-3*m_paddingBits+3]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-3*m_width-3*m_paddingBits+3]];
                break;
            case 30:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-3*m_width-3*m_paddingBits+2]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-3*m_width-3*m_paddingBits+2]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-3*m_width-3*m_paddingBits+2]];
                break;
            case 31:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-3*m_width-3*m_paddingBits+1]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-3*m_width-3*m_paddingBits+1]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-3*m_width-3*m_paddingBits+1]];
                break;
            case 32:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-3*m_width-3*m_paddingBits]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-3*m_width-3*m_paddingBits]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-3*m_width-3*m_paddingBits]];
                break;
            case 33:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-3*m_width-3*m_paddingBits-1]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-3*m_width-3*m_paddingBits-1]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-3*m_width-3*m_paddingBits-1]];
                break;
            case 34:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-3*m_width-3*m_paddingBits-2]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-3*m_width-3*m_paddingBits-2]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-3*m_width-3*m_paddingBits-2]];
                break;
            case 35:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-3*m_width-3*m_paddingBits-3]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-3*m_width-3*m_paddingBits-3]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-3*m_width-3*m_paddingBits-3]];
                break;
            case 36:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-2*m_width-2*m_paddingBits-3]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-2*m_width-2*m_paddingBits-3]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-2*m_width-2*m_paddingBits-3]];
                break;
            case 37:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-m_width-m_paddingBits-3]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-m_width-m_paddingBits-3]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-m_width-m_paddingBits-3]];
                break;
            case 38:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-3]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-3]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos-3]];
                break;
            case 39:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+m_width+m_paddingBits-3]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+m_width+m_paddingBits-3]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+m_width+m_paddingBits-3]];
                break;
            case 40:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+2*m_width+2*m_paddingBits-3]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+2*m_width+2*m_paddingBits-3]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+2*m_width+2*m_paddingBits-3]];
                break;
            case 41:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+3*m_width+3*m_paddingBits-3]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+3*m_width+3*m_paddingBits-3]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+3*m_width+3*m_paddingBits-3]];
                break;
            case 42:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+3*m_width+3*m_paddingBits-2]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+3*m_width+3*m_paddingBits-2]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+3*m_width+3*m_paddingBits-2]];
                break;
            case 43:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+3*m_width+3*m_paddingBits-1]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+3*m_width+3*m_paddingBits-1]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+3*m_width+3*m_paddingBits-1]];
                break;
            case 44:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+3*m_width+3*m_paddingBits]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+3*m_width+3*m_paddingBits]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+3*m_width+3*m_paddingBits]];
                break;
            case 45:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+3*m_width+3*m_paddingBits+1]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+3*m_width+3*m_paddingBits+1]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+3*m_width+3*m_paddingBits+1]];
                break;
            case 46:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+3*m_width+3*m_paddingBits+2]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+3*m_width+3*m_paddingBits+2]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+3*m_width+3*m_paddingBits+2]];
                break;
            case 47:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+3*m_width+3*m_paddingBits+3]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+3*m_width+3*m_paddingBits+3]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+3*m_width+3*m_paddingBits+3]];
                break;
            case 48:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+2*m_width+2*m_paddingBits+3]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+2*m_width+2*m_paddingBits+3]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+2*m_width+2*m_paddingBits+3]];
                break;
            case 49:
                bufferRed[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+m_width+m_paddingBits+3]+2];
                bufferGreen[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+m_width+m_paddingBits+3]+1];
                bufferBlue[i]=m_fileContent[m_palettePos+4*m_fileContent[currentPos+m_width+m_paddingBits+3]];
                break;
            }
        }
        else if(m_bitCount==24)
        {
            switch(locations[i])
            {
            case 1:
                bufferRed[i]=m_fileContent[currentPos+2];
                bufferGreen[i]=m_fileContent[currentPos+1];
                bufferBlue[i]=m_fileContent[currentPos];
                break;
            case 2:
                bufferRed[i]=m_fileContent[currentPos+1*3+2];
                bufferGreen[i]=m_fileContent[currentPos+1*3+1];
                bufferBlue[i]=m_fileContent[currentPos+1*3];
                break;
            case 3:
                bufferRed[i]=m_fileContent[currentPos-m_width*3-m_paddingBits*3+1*3+2];
                bufferGreen[i]=m_fileContent[currentPos-m_width*3-m_paddingBits*3+1*3+1];
                bufferBlue[i]=m_fileContent[currentPos-m_width*3-m_paddingBits*3+1*3];
                break;
            case 4:
                bufferRed[i]=m_fileContent[currentPos-m_width*3-m_paddingBits*3+2];
                bufferGreen[i]=m_fileContent[currentPos-m_width*3-m_paddingBits*3+1];
                bufferBlue[i]=m_fileContent[currentPos-m_width*3-m_paddingBits*3];
                break;
            case 5:
                bufferRed[i]=m_fileContent[currentPos-m_width*3-m_paddingBits*3-1*3+2];
                bufferGreen[i]=m_fileContent[currentPos-m_width*3-m_paddingBits*3-1*3+1];
                bufferBlue[i]=m_fileContent[currentPos-m_width*3-m_paddingBits*3-1*3];
                break;
            case 6:
                bufferRed[i]=m_fileContent[currentPos-1*3+2];
                bufferGreen[i]=m_fileContent[currentPos-1*3+1];
                bufferBlue[i]=m_fileContent[currentPos-1*3];
                break;
            case 7:
                bufferRed[i]=m_fileContent[currentPos+m_width*3+m_paddingBits*3-1*3+2];
                bufferGreen[i]=m_fileContent[currentPos+m_width*3+m_paddingBits*3-1*3+1];
                bufferBlue[i]=m_fileContent[currentPos+m_width*3+m_paddingBits*3-1*3];
                break;
            case 8:
                bufferRed[i]=m_fileContent[currentPos+m_width*3+m_paddingBits*3+2];
                bufferGreen[i]=m_fileContent[currentPos+m_width*3+m_paddingBits*3+1];
                bufferBlue[i]=m_fileContent[currentPos+m_width*3+m_paddingBits*3];
                break;
            case 9:
                bufferRed[i]=m_fileContent[currentPos+m_width*3+m_paddingBits*3+1*3+2];
                bufferGreen[i]=m_fileContent[currentPos+m_width*3+m_paddingBits*3+1*3+1];
                bufferBlue[i]=m_fileContent[currentPos+m_width*3+m_paddingBits*3+1*3];
                break;
            case 10:
                bufferRed[i]=m_fileContent[currentPos+2*3+2];
                bufferGreen[i]=m_fileContent[currentPos+2*3+1];
                bufferBlue[i]=m_fileContent[currentPos+2*3];
                break;
            case 11:
                bufferRed[i]=m_fileContent[currentPos-m_width*3-m_paddingBits*3+2*3+2];
                bufferGreen[i]=m_fileContent[currentPos-m_width*3-m_paddingBits*3+2*3+1];
                bufferBlue[i]=m_fileContent[currentPos-m_width*3-m_paddingBits*3+2*3];
                break;
            case 12:
                bufferRed[i]=m_fileContent[currentPos-2*m_width*3-2*m_paddingBits*3+2*3+2];
                bufferGreen[i]=m_fileContent[currentPos-2*m_width*3-2*m_paddingBits*3+2*3+1];
                bufferBlue[i]=m_fileContent[currentPos-2*m_width*3-2*m_paddingBits*3+2*3];
                break;
            case 13:
                bufferRed[i]=m_fileContent[currentPos-2*m_width*3-2*m_paddingBits*3+1*3+2];
                bufferGreen[i]=m_fileContent[currentPos-2*m_width*3-2*m_paddingBits*3+1*3+1];
                bufferBlue[i]=m_fileContent[currentPos-2*m_width*3-2*m_paddingBits*3+1*3];
                break;
            case 14:
                bufferRed[i]=m_fileContent[currentPos-2*m_width*3-2*m_paddingBits*3+2];
                bufferGreen[i]=m_fileContent[currentPos-2*m_width*3-2*m_paddingBits*3+1];
                bufferBlue[i]=m_fileContent[currentPos-2*m_width*3-2*m_paddingBits*3];
                break;
            case 15:
                bufferRed[i]=m_fileContent[currentPos-2*m_width*3-2*m_paddingBits*3-1*3+2];
                bufferGreen[i]=m_fileContent[currentPos-2*m_width*3-2*m_paddingBits*3-1*3+1];
                bufferBlue[i]=m_fileContent[currentPos-2*m_width*3-2*m_paddingBits*3-1*3];
                break;
            case 16:
                bufferRed[i]=m_fileContent[currentPos-2*m_width*3-2*m_paddingBits*3-2*3+2];
                bufferGreen[i]=m_fileContent[currentPos-2*m_width*3-2*m_paddingBits*3-2*3+1];
                bufferBlue[i]=m_fileContent[currentPos-2*m_width*3-2*m_paddingBits*3-2*3];
                break;
            case 17:
                bufferRed[i]=m_fileContent[currentPos-m_width*3-m_paddingBits*3-2*3+2];
                bufferGreen[i]=m_fileContent[currentPos-m_width*3-m_paddingBits*3-2*3+1];
                bufferBlue[i]=m_fileContent[currentPos-m_width*3-m_paddingBits*3-2*3];
                break;
            case 18:
                bufferRed[i]=m_fileContent[currentPos-2*3+2];
                bufferGreen[i]=m_fileContent[currentPos-2*3+1];
                bufferBlue[i]=m_fileContent[currentPos-2*3];
                break;
            case 19:
                bufferRed[i]=m_fileContent[currentPos+m_width*3+m_paddingBits*3-2*3+2];
                bufferGreen[i]=m_fileContent[currentPos+m_width*3+m_paddingBits*3-2*3+1];
                bufferBlue[i]=m_fileContent[currentPos+m_width*3+m_paddingBits*3-2*3];
                break;
            case 20:
                bufferRed[i]=m_fileContent[currentPos+2*m_width*3+2*m_paddingBits*3-2*3+2];
                bufferGreen[i]=m_fileContent[currentPos+2*m_width*3+2*m_paddingBits*3-2*3+1];
                bufferBlue[i]=m_fileContent[currentPos+2*m_width*3+2*m_paddingBits*3-2*3];
                break;
            case 21:
                bufferRed[i]=m_fileContent[currentPos+2*m_width*3+2*m_paddingBits*3-1*3+2];
                bufferGreen[i]=m_fileContent[currentPos+2*m_width*3+2*m_paddingBits*3-1*3+1];
                bufferBlue[i]=m_fileContent[currentPos+2*m_width*3+2*m_paddingBits*3-1*3];
                break;
            case 22:
                bufferRed[i]=m_fileContent[currentPos+2*m_width*3+2*m_paddingBits*3+2];
                bufferGreen[i]=m_fileContent[currentPos+2*m_width*3+2*m_paddingBits*3+1];
                bufferBlue[i]=m_fileContent[currentPos+2*m_width*3+2*m_paddingBits*3];
                break;
            case 23:
                bufferRed[i]=m_fileContent[currentPos+2*m_width*3+2*m_paddingBits*3+1*3+2];
                bufferGreen[i]=m_fileContent[currentPos+2*m_width*3+2*m_paddingBits*3+1*3+1];
                bufferBlue[i]=m_fileContent[currentPos+2*m_width*3+2*m_paddingBits*3+1*3];
                break;
            case 24:
                bufferRed[i]=m_fileContent[currentPos+2*m_width*3+2*m_paddingBits*3+2*3+2];
                bufferGreen[i]=m_fileContent[currentPos+2*m_width*3+2*m_paddingBits*3+2*3+1];
                bufferBlue[i]=m_fileContent[currentPos+2*m_width*3+2*m_paddingBits*3+2*3];
                break;
            case 25:
                bufferRed[i]=m_fileContent[currentPos+m_width*3+m_paddingBits*3+2*3+2];
                bufferGreen[i]=m_fileContent[currentPos+m_width*3+m_paddingBits*3+2*3+1];
                bufferBlue[i]=m_fileContent[currentPos+m_width*3+m_paddingBits*3+2*3];
                break;
            case 26:
                bufferRed[i]=m_fileContent[currentPos+3*3+2];
                bufferGreen[i]=m_fileContent[currentPos+3*3+1];
                bufferBlue[i]=m_fileContent[currentPos+3*3];
                break;
            case 27:
                bufferRed[i]=m_fileContent[currentPos-m_width*3-m_paddingBits*3+3*3+2];
                bufferGreen[i]=m_fileContent[currentPos-m_width*3-m_paddingBits*3+3*3+1];
                bufferBlue[i]=m_fileContent[currentPos-m_width*3-m_paddingBits*3+3*3];
                break;
            case 28:
                bufferRed[i]=m_fileContent[currentPos-2*m_width*3-2*m_paddingBits*3+3*3+2];
                bufferGreen[i]=m_fileContent[currentPos-2*m_width*3-2*m_paddingBits*3+3*3+1];
                bufferBlue[i]=m_fileContent[currentPos-2*m_width*3-2*m_paddingBits*3+3*3];
                break;
            case 29:
                bufferRed[i]=m_fileContent[currentPos-3*m_width*3-3*m_paddingBits*3+3*3+2];
                bufferGreen[i]=m_fileContent[currentPos-3*m_width*3-3*m_paddingBits*3+3*3+1];
                bufferBlue[i]=m_fileContent[currentPos-3*m_width*3-3*m_paddingBits*3+3*3];
                break;
            case 30:
                bufferRed[i]=m_fileContent[currentPos-3*m_width*3-3*m_paddingBits*3+2*3+2];
                bufferGreen[i]=m_fileContent[currentPos-3*m_width*3-3*m_paddingBits*3+2*3+1];
                bufferBlue[i]=m_fileContent[currentPos-3*m_width*3-3*m_paddingBits*3+2*3];
                break;
            case 31:
                bufferRed[i]=m_fileContent[currentPos-3*m_width*3-3*m_paddingBits*3+1*3+2];
                bufferGreen[i]=m_fileContent[currentPos-3*m_width*3-3*m_paddingBits*3+1*3+1];
                bufferBlue[i]=m_fileContent[currentPos-3*m_width*3-3*m_paddingBits*3+1*3];
                break;
            case 32:
                bufferRed[i]=m_fileContent[currentPos-3*m_width*3-3*m_paddingBits*3+2];
                bufferGreen[i]=m_fileContent[currentPos-3*m_width*3-3*m_paddingBits*3+1];
                bufferBlue[i]=m_fileContent[currentPos-3*m_width*3-3*m_paddingBits*3];
                break;
            case 33:
                bufferRed[i]=m_fileContent[currentPos-3*m_width*3-3*m_paddingBits*3-1*3+2];
                bufferGreen[i]=m_fileContent[currentPos-3*m_width*3-3*m_paddingBits*3-1*3+1];
                bufferBlue[i]=m_fileContent[currentPos-3*m_width*3-3*m_paddingBits*3-1*3];
                break;
            case 34:
                bufferRed[i]=m_fileContent[currentPos-3*m_width*3-3*m_paddingBits*3-2*3+2];
                bufferGreen[i]=m_fileContent[currentPos-3*m_width*3-3*m_paddingBits*3-2*3+1];
                bufferBlue[i]=m_fileContent[currentPos-3*m_width*3-3*m_paddingBits*3-2*3];
                break;
            case 35:
                bufferRed[i]=m_fileContent[currentPos-3*m_width*3-3*m_paddingBits*3-3*3+2];
                bufferGreen[i]=m_fileContent[currentPos-3*m_width*3-3*m_paddingBits*3-3*3+1];
                bufferBlue[i]=m_fileContent[currentPos-3*m_width*3-3*m_paddingBits*3-3*3];
                break;
            case 36:
                bufferRed[i]=m_fileContent[currentPos-2*m_width*3-2*m_paddingBits*3-3*3+2];
                bufferGreen[i]=m_fileContent[currentPos-2*m_width*3-2*m_paddingBits*3-3*3+1];
                bufferBlue[i]=m_fileContent[currentPos-2*m_width*3-2*m_paddingBits*3-3*3];
                break;
            case 37:
                bufferRed[i]=m_fileContent[currentPos-m_width*3-m_paddingBits*3-3*3+2];
                bufferGreen[i]=m_fileContent[currentPos-m_width*3-m_paddingBits*3-3*3+1];
                bufferBlue[i]=m_fileContent[currentPos-m_width*3-m_paddingBits*3-3*3];
                break;
            case 38:
                bufferRed[i]=m_fileContent[currentPos-3*3+2];
                bufferGreen[i]=m_fileContent[currentPos-3*3+1];
                bufferBlue[i]=m_fileContent[currentPos-3*3];
                break;
            case 39:
                bufferRed[i]=m_fileContent[currentPos+m_width*3+m_paddingBits*3-3*3+2];
                bufferGreen[i]=m_fileContent[currentPos+m_width*3+m_paddingBits*3-3*3+1];
                bufferBlue[i]=m_fileContent[currentPos+m_width*3+m_paddingBits*3-3*3];
                break;
            case 40:
                bufferRed[i]=m_fileContent[currentPos+2*m_width*3+2*m_paddingBits*3-3*3+2];
                bufferGreen[i]=m_fileContent[currentPos+2*m_width*3+2*m_paddingBits*3-3*3+1];
                bufferBlue[i]=m_fileContent[currentPos+2*m_width*3+2*m_paddingBits*3-3*3];
                break;
            case 41:
                bufferRed[i]=m_fileContent[currentPos+3*m_width*3+3*m_paddingBits*3-3*3+2];
                bufferGreen[i]=m_fileContent[currentPos+3*m_width*3+3*m_paddingBits*3-3*3+1];
                bufferBlue[i]=m_fileContent[currentPos+3*m_width*3+3*m_paddingBits*3-3*3];
                break;
            case 42:
                bufferRed[i]=m_fileContent[currentPos+3*m_width*3+3*m_paddingBits*3-2*3+2];
                bufferGreen[i]=m_fileContent[currentPos+3*m_width*3+3*m_paddingBits*3-2*3+1];
                bufferBlue[i]=m_fileContent[currentPos+3*m_width*3+3*m_paddingBits*3-2*3];
                break;
            case 43:
                bufferRed[i]=m_fileContent[currentPos+3*m_width*3+3*m_paddingBits*3-1*3+2];
                bufferGreen[i]=m_fileContent[currentPos+3*m_width*3+3*m_paddingBits*3-1*3+1];
                bufferBlue[i]=m_fileContent[currentPos+3*m_width*3+3*m_paddingBits*3-1*3];
                break;
            case 44:
                bufferRed[i]=m_fileContent[currentPos+3*m_width*3+3*m_paddingBits*3+2];
                bufferGreen[i]=m_fileContent[currentPos+3*m_width*3+3*m_paddingBits*3+1];
                bufferBlue[i]=m_fileContent[currentPos+3*m_width*3+3*m_paddingBits*3];
                break;
            case 45:
                bufferRed[i]=m_fileContent[currentPos+3*m_width*3+3*m_paddingBits*3+1*3+2];
                bufferGreen[i]=m_fileContent[currentPos+3*m_width*3+3*m_paddingBits*3+1*3+1];
                bufferBlue[i]=m_fileContent[currentPos+3*m_width*3+3*m_paddingBits*3+1*3];
                break;
            case 46:
                bufferRed[i]=m_fileContent[currentPos+3*m_width*3+3*m_paddingBits*3+2*3+2];
                bufferGreen[i]=m_fileContent[currentPos+3*m_width*3+3*m_paddingBits*3+2*3+1];
                bufferBlue[i]=m_fileContent[currentPos+3*m_width*3+3*m_paddingBits*3+2*3];
                break;
            case 47:
                bufferRed[i]=m_fileContent[currentPos+3*m_width*3+3*m_paddingBits*3+3*3+2];
                bufferGreen[i]=m_fileContent[currentPos+3*m_width*3+3*m_paddingBits*3+3*3+1];
                bufferBlue[i]=m_fileContent[currentPos+3*m_width*3+3*m_paddingBits*3+3*3];
                break;
            case 48:
                bufferRed[i]=m_fileContent[currentPos+2*m_width*3+2*m_paddingBits*3+3*3+2];
                bufferGreen[i]=m_fileContent[currentPos+2*m_width*3+2*m_paddingBits*3+3*3+1];
                bufferBlue[i]=m_fileContent[currentPos+2*m_width*3+2*m_paddingBits*3+3*3];
                break;
            case 49:
                bufferRed[i]=m_fileContent[currentPos+m_width*3+m_paddingBits*3+3*3+2];
                bufferGreen[i]=m_fileContent[currentPos+m_width*3+m_paddingBits*3+3*3+1];
                bufferBlue[i]=m_fileContent[currentPos+m_width*3+m_paddingBits*3+3*3];
                break;
            }
        }
    }

    *r=this->FindMedian(bufferRed,count);
    *g=this->FindMedian(bufferGreen,count);
    *b=this->FindMedian(bufferBlue,count);

    if(ifMedianInRange!=0 && ifThisInRange!=0)
    {
        if(*r>bufferRed[0] && *r<bufferRed[count-1] &&
                *g>bufferGreen[0] && *g<bufferGreen[count-1] &&
                *b>bufferBlue[0] && *b<bufferBlue[count-1])
            *ifMedianInRange=true;
        else
            *ifMedianInRange=false;

        if(m_bitCount==8)
        {
            if(m_fileContent[m_palettePos+4*m_fileContent[currentPos]+2]>bufferRed[0]
                    && m_fileContent[m_palettePos+4*m_fileContent[currentPos]+2]<bufferRed[count-1]
                    && m_fileContent[m_palettePos+4*m_fileContent[currentPos]+1]>bufferGreen[0]
                    && m_fileContent[m_palettePos+4*m_fileContent[currentPos]+1]<bufferGreen[count-1]
                    && m_fileContent[m_palettePos+4*m_fileContent[currentPos]]>bufferBlue[0]
                    && m_fileContent[m_palettePos+4*m_fileContent[currentPos]]<bufferBlue[count-1])
                *ifThisInRange=true;
            else
                *ifThisInRange=false;
        }
        else if(m_bitCount==24)
        {
            if(m_fileContent[currentPos+2]>bufferRed[0] && m_fileContent[currentPos+2]<bufferRed[count-1]
                    && m_fileContent[currentPos+1]>bufferGreen[0] && m_fileContent[currentPos+1]<bufferGreen[count-1]
                    && m_fileContent[currentPos]>bufferBlue[0] && m_fileContent[currentPos]<bufferBlue[count-1])
                *ifThisInRange=true;
            else
                *ifThisInRange=false;
        }
    }
}

void ImageWidget::onRestore()
{
    if(m_isDirty)
    {
        memcpy(m_fileContent,m_fileBackup,m_fileSize);
        update();
    }
}
