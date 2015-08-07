#ifndef MYSLIDER_H
#define MYSLIDER_H

#include <QtGlobal>
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#include <QSlider>
#include <QtGui>
#else
#include <QtWidgets>
#endif

class mySlider : public QSlider
{
  Q_OBJECT

 public:

    explicit mySlider(QFrame * parent = 0):
      QSlider(parent)
    {
    }

  
protected:

  void mousePressEvent(QMouseEvent * event)
  {
    if (event->button() == Qt::RightButton && event->type() == QEvent::MouseButtonDblClick) {
        setValue(0);
        event->accept();
    }
    QSlider::mousePressEvent(event);
  }
};

#endif
