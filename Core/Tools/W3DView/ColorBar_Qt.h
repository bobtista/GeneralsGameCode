#pragma once

#include <QWidget>
#include <QColor>
#include <QPoint>
#include <QVector>

#define CBRS_SUNKEN 0x00000001
#define CBRS_RAISED 0x00000002
#define CBRS_FRAME 0x00000004
#define CBRS_HORZ 0x00000008
#define CBRS_VERT 0x00000010
#define CBRS_HAS_SEL 0x00000020
#define CBRS_SHOW_FRAMES 0x00000040
#define CBRS_PAINT_GRAPH 0x00000080

#define POINT_VISIBLE 0x00000001
#define POINT_CAN_MOVE 0x00000002

#define CBRN_MOVED_POINT 0x0001
#define CBRN_MOVING_POINT 0x0002
#define CBRN_DBLCLK_POINT 0x0003
#define CBRN_SEL_CHANGED 0x0004
#define CBRN_DEL_POINT 0x0005
#define CBRN_DELETED_POINT 0x0006
#define CBRN_INSERTED_POINT 0x0007

const int MAX_COLOR_POINTS = 15;

struct COLOR_POINT
{
	float PosPercent;
	int StartPos;
	int EndPos;
	
	float StartGraphPercent;
	float GraphPercentInc;
	
	float StartRed;
	float StartGreen;
	float StartBlue;
	
	float RedInc;
	float GreenInc;
	float BlueInc;
	
	unsigned int user_data;
	int flags;
};

class ColorBarClass : public QWidget
{
	Q_OBJECT

public:
	explicit ColorBarClass(QWidget* parent = nullptr);
	~ColorBarClass();

	bool Insert_Point(int index, float position, float red, float green, float blue, int flags = POINT_VISIBLE | POINT_CAN_MOVE);
	bool Insert_Point(QPoint point, int flags = POINT_VISIBLE | POINT_CAN_MOVE);
	bool Modify_Point(int index, float position, float red, float green, float blue, int flags = POINT_VISIBLE | POINT_CAN_MOVE);
	bool Set_User_Data(int index, unsigned int data);
	unsigned int Get_User_Data(int index);
	bool Set_Graph_Percent(int index, float percent);
	float Get_Graph_Percent(int index);
	bool Delete_Point(int index);
	void Clear_Points();

	int Get_Point_Count() const { return m_iColorPoints; }
	bool Get_Point(int index, float* position, float* red, float* green, float* blue);

	int Marker_From_Point(QPoint point);
	void Set_Selection_Pos(float pos);
	float Get_Selection_Pos() const { return m_SelectionPos; }
	void Get_Color(float position, float* red, float* green, float* blue);

	void Get_Range(float& min, float& max) const { min = m_MinPos; max = m_MaxPos; }
	void Set_Range(float min, float max);

	void Set_Redraw(bool redraw = true);

signals:
	void PointMoved(int key_index, float red, float green, float blue, float position);
	void PointMoving(int key_index, float red, float green, float blue, float position);
	void PointDoubleClicked(int key_index);
	void SelectionChanged(int key_index);
	void PointDeleted(int key_index);
	void PointInserted(int key_index);

protected:
	void paintEvent(QPaintEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseDoubleClickEvent(QMouseEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;
	void focusInEvent(QFocusEvent* event) override;
	void focusOutEvent(QFocusEvent* event) override;

private:
	void Paint_Bar();
	void Update_Point_Info();
	void Get_Selection_Rectangle(QRect& rect);
	void Move_Selection(QPoint point, bool send_notify = true);
	void Move_Selection(float new_pos, bool send_notify = true);
	void Repaint();

	QPixmap* m_pixmap;
	int m_iColorWidth;
	int m_iColorHeight;
	int m_iColorPoints;
	float m_MinPos;
	float m_MaxPos;
	COLOR_POINT m_ColorPoints[MAX_COLOR_POINTS];
	QRect m_ColorArea;
	int m_iCurrentKey;
	bool m_bMoving;
	bool m_bMoved;
	bool m_bRedraw;
	float m_SelectionPos;
	bool m_bHorz;
};



