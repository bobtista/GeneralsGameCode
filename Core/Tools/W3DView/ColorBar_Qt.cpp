#include "ColorBar_Qt.h"
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QPainter>
#include <QPixmap>
#include <QImage>
#include <QDebug>

ColorBarClass::ColorBarClass(QWidget* parent) :
	QWidget(parent),
	m_pixmap(nullptr),
	m_iColorWidth(0),
	m_iColorHeight(0),
	m_iColorPoints(0),
	m_MinPos(0.0f),
	m_MaxPos(1.0f),
	m_iCurrentKey(-1),
	m_bMoving(false),
	m_bMoved(false),
	m_bRedraw(true),
	m_SelectionPos(0.0f),
	m_bHorz(true)
{
	setMinimumSize(200, 50);
	setMouseTracking(true);
	setFocusPolicy(Qt::StrongFocus);
	
	memset(m_ColorPoints, 0, sizeof(m_ColorPoints));
}

ColorBarClass::~ColorBarClass()
{
	if (m_pixmap)
	{
		delete m_pixmap;
		m_pixmap = nullptr;
	}
}

bool ColorBarClass::Insert_Point(int index, float position, float red, float green, float blue, int flags)
{
	if (m_iColorPoints >= MAX_COLOR_POINTS || index < 0 || index > m_iColorPoints)
		return false;
	
	for (int i = m_iColorPoints; i > index; i--)
	{
		m_ColorPoints[i] = m_ColorPoints[i - 1];
	}
	
	m_ColorPoints[index].PosPercent = position;
	m_ColorPoints[index].StartRed = red;
	m_ColorPoints[index].StartGreen = green;
	m_ColorPoints[index].StartBlue = blue;
	m_ColorPoints[index].flags = flags;
	m_ColorPoints[index].user_data = 0;
	m_ColorPoints[index].StartGraphPercent = 0.0f;
	m_ColorPoints[index].GraphPercentInc = 0.0f;
	
	m_iColorPoints++;
	Update_Point_Info();
	update();
	
	emit PointInserted(index);
	return true;
}

bool ColorBarClass::Insert_Point(QPoint point, int flags)
{
	float position = m_bHorz ? 
		float(point.x() - m_ColorArea.left()) / m_ColorArea.width() :
		float(point.y() - m_ColorArea.top()) / m_ColorArea.height();
	
	position = position * (m_MaxPos - m_MinPos) + m_MinPos;
	
	int index = 0;
	for (int i = 0; i < m_iColorPoints; i++)
	{
		if (m_ColorPoints[i].PosPercent > position)
			break;
		index++;
	}
	
	QColor color = QColor(128, 128, 128);
	return Insert_Point(index, position, color.redF(), color.greenF(), color.blueF(), flags);
}

bool ColorBarClass::Modify_Point(int index, float position, float red, float green, float blue, int flags)
{
	if (index < 0 || index >= m_iColorPoints)
		return false;
	
	m_ColorPoints[index].PosPercent = position;
	m_ColorPoints[index].StartRed = red;
	m_ColorPoints[index].StartGreen = green;
	m_ColorPoints[index].StartBlue = blue;
	m_ColorPoints[index].flags = flags;
	
	Update_Point_Info();
	update();
	return true;
}

bool ColorBarClass::Set_User_Data(int index, unsigned int data)
{
	if (index < 0 || index >= m_iColorPoints)
		return false;
	
	m_ColorPoints[index].user_data = data;
	return true;
}

unsigned int ColorBarClass::Get_User_Data(int index)
{
	if (index < 0 || index >= m_iColorPoints)
		return 0;
	
	return m_ColorPoints[index].user_data;
}

bool ColorBarClass::Set_Graph_Percent(int index, float percent)
{
	if (index < 0 || index >= m_iColorPoints)
		return false;
	
	m_ColorPoints[index].StartGraphPercent = percent;
	Update_Point_Info();
	update();
	return true;
}

float ColorBarClass::Get_Graph_Percent(int index)
{
	if (index < 0 || index >= m_iColorPoints)
		return 0.0f;
	
	return m_ColorPoints[index].StartGraphPercent;
}

bool ColorBarClass::Delete_Point(int index)
{
	if (index < 0 || index >= m_iColorPoints)
		return false;
	
	emit PointDeleted(index);
	
	for (int i = index; i < m_iColorPoints - 1; i++)
	{
		m_ColorPoints[i] = m_ColorPoints[i + 1];
	}
	
	m_iColorPoints--;
	Update_Point_Info();
	update();
	return true;
}

void ColorBarClass::Clear_Points()
{
	m_iColorPoints = 0;
	m_iCurrentKey = -1;
	update();
}

bool ColorBarClass::Get_Point(int index, float* position, float* red, float* green, float* blue)
{
	if (index < 0 || index >= m_iColorPoints)
		return false;
	
	if (position) *position = m_ColorPoints[index].PosPercent;
	if (red) *red = m_ColorPoints[index].StartRed;
	if (green) *green = m_ColorPoints[index].StartGreen;
	if (blue) *blue = m_ColorPoints[index].StartBlue;
	return true;
}

int ColorBarClass::Marker_From_Point(QPoint point)
{
	for (int i = 0; i < m_iColorPoints; i++)
	{
		int x = m_ColorArea.left() + int((m_ColorPoints[i].PosPercent - m_MinPos) / (m_MaxPos - m_MinPos) * m_ColorArea.width());
		int y = m_ColorArea.top() + m_ColorArea.height() / 2;
		
		QRect markerRect(x - 5, y - 10, 10, 20);
		if (markerRect.contains(point))
			return i;
	}
	return -1;
}

void ColorBarClass::Set_Selection_Pos(float pos)
{
	m_SelectionPos = pos;
	update();
}

void ColorBarClass::Get_Color(float position, float* red, float* green, float* blue)
{
	if (m_iColorPoints == 0)
	{
		if (red) *red = 0.0f;
		if (green) *green = 0.0f;
		if (blue) *blue = 0.0f;
		return;
	}
	
	if (position <= m_ColorPoints[0].PosPercent)
	{
		if (red) *red = m_ColorPoints[0].StartRed;
		if (green) *green = m_ColorPoints[0].StartGreen;
		if (blue) *blue = m_ColorPoints[0].StartBlue;
		return;
	}
	
	if (position >= m_ColorPoints[m_iColorPoints - 1].PosPercent)
	{
		if (red) *red = m_ColorPoints[m_iColorPoints - 1].StartRed;
		if (green) *green = m_ColorPoints[m_iColorPoints - 1].StartGreen;
		if (blue) *blue = m_ColorPoints[m_iColorPoints - 1].StartBlue;
		return;
	}
	
	for (int i = 0; i < m_iColorPoints - 1; i++)
	{
		if (position >= m_ColorPoints[i].PosPercent && position <= m_ColorPoints[i + 1].PosPercent)
		{
			float t = (position - m_ColorPoints[i].PosPercent) / 
			          (m_ColorPoints[i + 1].PosPercent - m_ColorPoints[i].PosPercent);
			
			if (red) *red = m_ColorPoints[i].StartRed + t * (m_ColorPoints[i + 1].StartRed - m_ColorPoints[i].StartRed);
			if (green) *green = m_ColorPoints[i].StartGreen + t * (m_ColorPoints[i + 1].StartGreen - m_ColorPoints[i].StartGreen);
			if (blue) *blue = m_ColorPoints[i].StartBlue + t * (m_ColorPoints[i + 1].StartBlue - m_ColorPoints[i].StartBlue);
			return;
		}
	}
}

void ColorBarClass::Set_Range(float min, float max)
{
	m_MinPos = min;
	m_MaxPos = max;
	Update_Point_Info();
	update();
}

void ColorBarClass::Set_Redraw(bool redraw)
{
	m_bRedraw = redraw;
	if (redraw)
		update();
}

void ColorBarClass::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);
	
	if (m_pixmap && !m_pixmap->isNull())
	{
		painter.drawPixmap(0, 0, *m_pixmap);
	}
	else
	{
		painter.fillRect(rect(), palette().color(QPalette::Window));
	}
	
	Paint_Bar();
}

void ColorBarClass::resizeEvent(QResizeEvent* event)
{
	m_iColorWidth = width();
	m_iColorHeight = height();
	
	int margin = 20;
	m_ColorArea = QRect(margin, margin, m_iColorWidth - 2 * margin, m_iColorHeight - 2 * margin);
	
	if (m_pixmap)
	{
		delete m_pixmap;
		m_pixmap = nullptr;
	}
	
	m_pixmap = new QPixmap(m_iColorWidth, m_iColorHeight);
	Paint_Bar();
	
	QWidget::resizeEvent(event);
}

void ColorBarClass::mousePressEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		int key = Marker_From_Point(event->pos());
		if (key >= 0)
		{
			m_iCurrentKey = key;
			m_bMoving = true;
			m_bMoved = false;
		}
		else
		{
			Move_Selection(event->pos(), true);
		}
	}
	QWidget::mousePressEvent(event);
}

void ColorBarClass::mouseReleaseEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		if (m_bMoving && m_bMoved)
		{
			emit PointMoved(m_iCurrentKey, 
			                m_ColorPoints[m_iCurrentKey].StartRed,
			                m_ColorPoints[m_iCurrentKey].StartGreen,
			                m_ColorPoints[m_iCurrentKey].StartBlue,
			                m_ColorPoints[m_iCurrentKey].PosPercent);
		}
		m_bMoving = false;
		m_bMoved = false;
	}
	QWidget::mouseReleaseEvent(event);
}

void ColorBarClass::mouseMoveEvent(QMouseEvent* event)
{
	if (m_bMoving && m_iCurrentKey >= 0)
	{
		Move_Selection(event->pos(), true);
		m_bMoved = true;
		emit PointMoving(m_iCurrentKey,
		                 m_ColorPoints[m_iCurrentKey].StartRed,
		                 m_ColorPoints[m_iCurrentKey].StartGreen,
		                 m_ColorPoints[m_iCurrentKey].StartBlue,
		                 m_ColorPoints[m_iCurrentKey].PosPercent);
	}
	QWidget::mouseMoveEvent(event);
}

void ColorBarClass::mouseDoubleClickEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		int key = Marker_From_Point(event->pos());
		if (key >= 0)
		{
			emit PointDoubleClicked(key);
		}
	}
	QWidget::mouseDoubleClickEvent(event);
}

void ColorBarClass::keyPressEvent(QKeyEvent* event)
{
	if (m_iCurrentKey >= 0 && event->key() == Qt::Key_Delete)
	{
		Delete_Point(m_iCurrentKey);
		m_iCurrentKey = -1;
	}
	QWidget::keyPressEvent(event);
}

void ColorBarClass::focusInEvent(QFocusEvent* event)
{
	QWidget::focusInEvent(event);
	update();
}

void ColorBarClass::focusOutEvent(QFocusEvent* event)
{
	QWidget::focusOutEvent(event);
	update();
}

void ColorBarClass::Paint_Bar()
{
	if (!m_pixmap || m_ColorArea.isEmpty())
		return;
	
	QImage image(m_iColorWidth, m_iColorHeight, QImage::Format_RGB32);
	image.fill(palette().color(QPalette::Window).rgb());
	
	QPainter painter(&image);
	
	if (m_iColorPoints > 0)
	{
		for (int x = 0; x < m_ColorArea.width(); x++)
		{
			float position = float(x) / m_ColorArea.width() * (m_MaxPos - m_MinPos) + m_MinPos;
			float red, green, blue;
			Get_Color(position, &red, &green, &blue);
			
			QColor color;
			color.setRgbF(red, green, blue);
			painter.setPen(color);
			painter.drawLine(m_ColorArea.left() + x, m_ColorArea.top(),
			                 m_ColorArea.left() + x, m_ColorArea.bottom());
		}
		
		for (int i = 0; i < m_iColorPoints; i++)
		{
			int x = m_ColorArea.left() + int((m_ColorPoints[i].PosPercent - m_MinPos) / (m_MaxPos - m_MinPos) * m_ColorArea.width());
			int y = m_ColorArea.top() + m_ColorArea.height() / 2;
			
			QColor pointColor;
			pointColor.setRgbF(m_ColorPoints[i].StartRed, m_ColorPoints[i].StartGreen, m_ColorPoints[i].StartBlue);
			
			painter.setPen(Qt::black);
			painter.setBrush(pointColor);
			painter.drawEllipse(QPoint(x, y), 6, 6);
		}
	}
	
	*m_pixmap = QPixmap::fromImage(image);
	update();
}

void ColorBarClass::Update_Point_Info()
{
	for (int i = 0; i < m_iColorPoints - 1; i++)
	{
		m_ColorPoints[i].StartPos = m_ColorArea.left() + int((m_ColorPoints[i].PosPercent - m_MinPos) / (m_MaxPos - m_MinPos) * m_ColorArea.width());
		m_ColorPoints[i].EndPos = m_ColorArea.left() + int((m_ColorPoints[i + 1].PosPercent - m_MinPos) / (m_MaxPos - m_MinPos) * m_ColorArea.width());
		
		float range = m_ColorPoints[i + 1].PosPercent - m_ColorPoints[i].PosPercent;
		if (range > 0.0f)
		{
			m_ColorPoints[i].RedInc = (m_ColorPoints[i + 1].StartRed - m_ColorPoints[i].StartRed) / range;
			m_ColorPoints[i].GreenInc = (m_ColorPoints[i + 1].StartGreen - m_ColorPoints[i].StartGreen) / range;
			m_ColorPoints[i].BlueInc = (m_ColorPoints[i + 1].StartBlue - m_ColorPoints[i].StartBlue) / range;
		}
	}
}

void ColorBarClass::Get_Selection_Rectangle(QRect& rect)
{
	if (m_iCurrentKey >= 0 && m_iCurrentKey < m_iColorPoints)
	{
		int x = m_ColorArea.left() + int((m_ColorPoints[m_iCurrentKey].PosPercent - m_MinPos) / (m_MaxPos - m_MinPos) * m_ColorArea.width());
		rect = QRect(x - 5, m_ColorArea.top(), 10, m_ColorArea.height());
	}
	else
	{
		rect = QRect();
	}
}

void ColorBarClass::Move_Selection(QPoint point, bool send_notify)
{
	if (m_iCurrentKey < 0 || m_iCurrentKey >= m_iColorPoints)
		return;
	
	float position = m_bHorz ?
		float(point.x() - m_ColorArea.left()) / m_ColorArea.width() :
		float(point.y() - m_ColorArea.top()) / m_ColorArea.height();
	
	position = position * (m_MaxPos - m_MinPos) + m_MinPos;
	
	if (position < m_MinPos) position = m_MinPos;
	if (position > m_MaxPos) position = m_MaxPos;
	
	if (m_iCurrentKey > 0 && position < m_ColorPoints[m_iCurrentKey - 1].PosPercent)
		position = m_ColorPoints[m_iCurrentKey - 1].PosPercent;
	if (m_iCurrentKey < m_iColorPoints - 1 && position > m_ColorPoints[m_iCurrentKey + 1].PosPercent)
		position = m_ColorPoints[m_iCurrentKey + 1].PosPercent;
	
	m_ColorPoints[m_iCurrentKey].PosPercent = position;
	Update_Point_Info();
	update();
}

void ColorBarClass::Move_Selection(float new_pos, bool send_notify)
{
	if (m_iCurrentKey < 0 || m_iCurrentKey >= m_iColorPoints)
		return;
	
	m_ColorPoints[m_iCurrentKey].PosPercent = new_pos;
	Update_Point_Info();
	update();
}

void ColorBarClass::Repaint()
{
	update();
}

