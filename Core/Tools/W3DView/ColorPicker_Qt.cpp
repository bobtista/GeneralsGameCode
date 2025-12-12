#include "ColorPicker_Qt.h"
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QImage>
#include <cmath>

ColorPickerClass::ColorPickerClass(QWidget* parent) :
	QWidget(parent),
	m_pixmap(nullptr),
	m_CurrentPoint(0, 0),
	m_CurrentColor(0, 0, 0),
	m_bSelecting(false),
	m_CurrentHue(0.0f),
	m_iWidth(0),
	m_iHeight(0)
{
	setMinimumSize(200, 200);
	setMouseTracking(true);
}

ColorPickerClass::~ColorPickerClass()
{
	Free_Bitmap();
}

void ColorPickerClass::Select_Color(int red, int green, int blue)
{
	m_CurrentColor = QColor(red, green, blue);
	m_CurrentPoint = Point_From_Color(m_CurrentColor);
	update();
}

void ColorPickerClass::Get_Current_Color(int* red, int* green, int* blue)
{
	if (red) *red = m_CurrentColor.red();
	if (green) *green = m_CurrentColor.green();
	if (blue) *blue = m_CurrentColor.blue();
}

void ColorPickerClass::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);
	
	if (m_pixmap && !m_pixmap->isNull())
	{
		painter.drawPixmap(0, 0, *m_pixmap);
	}
	else
	{
		painter.fillRect(rect(), Qt::white);
	}
	
	Paint_Marker();
}

void ColorPickerClass::resizeEvent(QResizeEvent* event)
{
	m_iWidth = width();
	m_iHeight = height();
	Create_Bitmap();
	QWidget::resizeEvent(event);
}

void ColorPickerClass::mousePressEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		m_bSelecting = true;
		QPoint pos = event->pos();
		m_CurrentColor = Color_From_Point(pos.x(), pos.y());
		m_CurrentPoint = pos;
		update();
		emit ColorChanged(m_CurrentColor.redF(), m_CurrentColor.greenF(), m_CurrentColor.blueF(), m_CurrentHue);
	}
	QWidget::mousePressEvent(event);
}

void ColorPickerClass::mouseReleaseEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		m_bSelecting = false;
	}
	QWidget::mouseReleaseEvent(event);
}

void ColorPickerClass::mouseMoveEvent(QMouseEvent* event)
{
	if (m_bSelecting && (event->buttons() & Qt::LeftButton))
	{
		QPoint pos = event->pos();
		m_CurrentColor = Color_From_Point(pos.x(), pos.y());
		m_CurrentPoint = pos;
		update();
		emit ColorChanged(m_CurrentColor.redF(), m_CurrentColor.greenF(), m_CurrentColor.blueF(), m_CurrentHue);
	}
	QWidget::mouseMoveEvent(event);
}

void ColorPickerClass::Create_Bitmap()
{
	Free_Bitmap();
	
	if (m_iWidth <= 0 || m_iHeight <= 0)
		return;
	
	m_pixmap = new QPixmap(m_iWidth, m_iHeight);
	Paint_DIB(m_iWidth, m_iHeight);
}

void ColorPickerClass::Free_Bitmap()
{
	if (m_pixmap)
	{
		delete m_pixmap;
		m_pixmap = nullptr;
	}
}

void ColorPickerClass::Paint_DIB(int width, int height)
{
	if (!m_pixmap)
		return;
	
	QImage image(width, height, QImage::Format_RGB32);
	
	QRect displayRect = Calc_Display_Rect();
	
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			if (displayRect.contains(x, y))
			{
				QColor color = Color_From_Point(x, y);
				image.setPixel(x, y, color.rgb());
			}
			else
			{
				image.setPixel(x, y, palette().color(QPalette::Window).rgb());
			}
		}
	}
	
	*m_pixmap = QPixmap::fromImage(image);
}

QColor ColorPickerClass::Color_From_Point(int x, int y)
{
	QRect displayRect = Calc_Display_Rect();
	
	if (!displayRect.contains(x, y))
		return m_CurrentColor;
	
	float normalizedX = float(x - displayRect.left()) / displayRect.width();
	float normalizedY = float(y - displayRect.top()) / displayRect.height();
	
	float hue = normalizedX * 360.0f;
	float saturation = normalizedY;
	float value = 1.0f;
	
	QColor color;
	color.setHsvF(hue / 360.0f, saturation, value);
	m_CurrentHue = hue;
	
	return color;
}

QPoint ColorPickerClass::Point_From_Color(const QColor& color)
{
	QRect displayRect = Calc_Display_Rect();
	
	float hue, saturation, value;
	color.getHsvF(&hue, &saturation, &value);
	
	int x = displayRect.left() + int(hue * displayRect.width());
	int y = displayRect.top() + int(saturation * displayRect.height());
	
	return QPoint(x, y);
}

void ColorPickerClass::Paint_Marker()
{
	QPainter painter(this);
	
	QPen pen(Qt::black, 2);
	painter.setPen(pen);
	
	int markerSize = 8;
	QRect markerRect(m_CurrentPoint.x() - markerSize / 2, 
	                 m_CurrentPoint.y() - markerSize / 2,
	                 markerSize, markerSize);
	
	painter.drawRect(markerRect);
	
	pen.setColor(Qt::white);
	painter.setPen(pen);
	painter.drawRect(markerRect.adjusted(1, 1, -1, -1));
}

void ColorPickerClass::Erase_Marker()
{
	update();
}

QRect ColorPickerClass::Calc_Display_Rect()
{
	int margin = 10;
	return QRect(margin, margin, m_iWidth - 2 * margin, m_iHeight - 2 * margin);
}



