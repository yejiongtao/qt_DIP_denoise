#ifndef IMAGE_WIDGET
#define IMAGE_WIDGET

#include <QWidget>
#include <QString>
#include <QPaintEvent>
#include <QFile>

class ImageWidget:public QWidget
{
    Q_OBJECT
public:
    ImageWidget(QString fileName,QWidget *parent=0);
    ~ImageWidget();

private:
    unsigned char FindMedian(unsigned char a[],int count);
    void GetMedianInAMask(unsigned char locations[],int currentPos,
                          unsigned char *r,unsigned char *g,unsigned char *b,
                          bool *ifMedianInRange=0,bool *ifThisInRange=0);

private:    
    QFile *m_file;
    QString m_fileName;
    unsigned char *m_fileContent;        //指向图片文件的内容
    unsigned char *m_fileBackup;         //文件内容备份
    qint64 m_fileSize;          //文件大小
    bool m_isDirty;             //标志内存中的图像是否被修改过了
    const int m_maxFilterSize;

    int m_offBits;              //储存像素信息的内容离文件开始的距离
    const int m_offBitsPos;     //该距离在bmp文件中的位置
    int m_width;
    const int m_widthPos;
    int m_height;
    const int m_heightPos;
    int m_bitCount;             //是多少位的bmp
    const int m_bitCountPos;
    const int m_palettePos;     //调色板位置

    int m_paddingBits;          //用来补齐m_width的bit数！
        //因为windows进行行扫描的时候最小单位是4字节，所以如果图片宽不是4的倍数，要用0,0,0补齐

protected:
    void paintEvent(QPaintEvent *e);

protected slots:
    void onMedianFiltering(int level);
    void onAdaptiveMedianFiltering();
    void onSave();
    void onSaveAs();
    void onRestore();
};

#endif // IMAGE_WIDGET

