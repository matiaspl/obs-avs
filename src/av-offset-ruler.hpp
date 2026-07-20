#pragma once

#include <QString>
#include <QWidget>
#include <cstdint>

class QPaintEvent;

class AVOffsetRuler : public QWidget {
public:
	explicit AVOffsetRuler(const QString &audio_early_label, const QString &audio_late_label,
			       QWidget *parent = nullptr);

	QSize minimumSizeHint() const override;
	QSize sizeHint() const override;
	void clearMeasurement();
	void setMeasurement(int64_t offset_ns, uint32_t fps_num, uint32_t fps_den);

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	QString audioEarlyLabel;
	QString audioLateLabel;
	bool hasMeasurement = false;
	int64_t offsetNs = 0;
	uint32_t fpsNum = 0;
	uint32_t fpsDen = 0;
};
