#pragma once

#include <QWidget>
#include <QColor>
#include <QPoint>

#define CPS_SUNKEN 0x00000001
#define CPS_RAISED 0x00000002

#define CPN_COLORCHANGE 0x0001

class ColorPickerClass : public QWidget
{
	Q_OBJECT

public:
	explicit ColorPickerClass(QWidget* parent = nullptr);
	~ColorPickerClass();

	void Select_Color(int red, int green, int blue);
	void Get_Current_Color(int* red, int* green, int* blue);

signals:
	void ColorChanged(float red, float green, float blue, float hue);

protected:
	void paintEvent(QPaintEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;

private:
	void Create_Bitmap();
	void Free_Bitmap();
	void Paint_DIB(int width, int height);
	QColor Color_From_Point(int x, int y);
	QPoint Point_From_Color(const QColor& color);
	void Paint_Marker();
	void Erase_Marker();
	QRect Calc_Display_Rect();

	QPixmap* m_pixmap;
	QPoint m_CurrentPoint;
	QColor m_CurrentColor;
	bool m_bSelecting;
	float m_CurrentHue;
	int m_iWidth;
	int m_iHeight;
};



