#pragma once

#include <QWidget>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QKeyEvent>

class CGraphicView : public QWidget
{
	Q_OBJECT

public:
	explicit CGraphicView(QWidget* parent = nullptr);
	~CGraphicView();

	float GetAnimationSpeed() const { return m_animationSpeed; }
	void SetAnimationSpeed(float animationSpeed) { m_animationSpeed = animationSpeed; }

protected:
	void paintEvent(QPaintEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;

private:
	void* m_renderContext;
	float m_animationSpeed;
};

