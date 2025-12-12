#include "GraphicView_Qt.h"
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QPaintEngine>
#include <QPainter>

#ifdef _WIN32
#include <windows.h>
#endif

CGraphicView::CGraphicView(QWidget* parent) :
	QWidget(parent),
	m_renderContext(nullptr),
	m_animationSpeed(1.0f)
{
	setFocusPolicy(Qt::StrongFocus);
	setAttribute(Qt::WA_PaintOnScreen, true);
	setAttribute(Qt::WA_NativeWindow, true);
}

CGraphicView::~CGraphicView()
{
}

void CGraphicView::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);
	painter.fillRect(rect(), Qt::black);
}

void CGraphicView::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
}

void CGraphicView::mousePressEvent(QMouseEvent* event)
{
	QWidget::mousePressEvent(event);
}

void CGraphicView::mouseMoveEvent(QMouseEvent* event)
{
	QWidget::mouseMoveEvent(event);
}

void CGraphicView::mouseReleaseEvent(QMouseEvent* event)
{
	QWidget::mouseReleaseEvent(event);
}

void CGraphicView::keyPressEvent(QKeyEvent* event)
{
	QWidget::keyPressEvent(event);
}

void CGraphicView::wheelEvent(QWheelEvent* event)
{
	QWidget::wheelEvent(event);
}

