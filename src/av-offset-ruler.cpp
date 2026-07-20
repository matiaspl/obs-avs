#include "av-offset-ruler.hpp"

#include <QPainter>
#include <algorithm>
#include <cmath>

static constexpr double RULER_MIN_MS = -40.0;
static constexpr double RULER_MAX_MS = 60.0;

AVOffsetRuler::AVOffsetRuler(const QString &audio_early_label, const QString &audio_late_label, QWidget *parent)
	: QWidget(parent), audioEarlyLabel(audio_early_label), audioLateLabel(audio_late_label)
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QSize AVOffsetRuler::minimumSizeHint() const
{
	return QSize(220, 78);
}

QSize AVOffsetRuler::sizeHint() const
{
	return QSize(360, 82);
}

void AVOffsetRuler::clearMeasurement()
{
	hasMeasurement = false;
	offsetNs = 0;
	update();
}

void AVOffsetRuler::setMeasurement(int64_t offset_ns, uint32_t fps_num, uint32_t fps_den)
{
	hasMeasurement = true;
	offsetNs = offset_ns;
	fpsNum = fps_num;
	fpsDen = fps_den;
	update();
}

void AVOffsetRuler::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	const qreal left = 10.0;
	const qreal right = std::max(left + 1.0, (qreal)width() - 10.0);
	const qreal axisY = 31.0;
	const qreal axisWidth = right - left;
	const auto valueToX = [left, axisWidth](double ms) {
		return left + (ms - RULER_MIN_MS) * axisWidth / (RULER_MAX_MS - RULER_MIN_MS);
	};

	const QPalette colors = palette();
	const QColor windowColor = colors.color(QPalette::Window);
	const QColor textColor = colors.color(QPalette::Text);
	const bool darkTheme = windowColor.lightness() < 128;
	QColor axisColor = colors.color(QPalette::Mid);
	if (darkTheme) {
		axisColor = textColor;
		axisColor.setAlphaF(0.65);
	}
	const QColor markerColor = darkTheme ? QColor(55, 185, 255) : QColor(0, 86, 185);
	painter.setPen(QPen(axisColor, 1.0));
	painter.drawLine(QPointF(left, axisY), QPointF(right, axisY));

	if (fpsNum && fpsDen) {
		const double halfFrameMs = 500.0 * (double)fpsDen / (double)fpsNum;
		const int firstTick = (int)std::ceil(RULER_MIN_MS / halfFrameMs);
		const int lastTick = (int)std::floor(RULER_MAX_MS / halfFrameMs);
		for (int tick = firstTick; tick <= lastTick; tick++) {
			const double valueMs = (double)tick * halfFrameMs;
			const qreal x = valueToX(valueMs);
			const bool wholeFrame = tick % 2 == 0;
			const qreal tickHeight = wholeFrame ? 9.0 : 5.0;
			painter.setPen(QPen(tick == 0 ? textColor : axisColor, tick == 0 ? 2.0 : 1.0));
			painter.drawLine(QPointF(x, axisY - tickHeight), QPointF(x, axisY + tickHeight));

			if (wholeFrame) {
				const int frame = tick / 2;
				QString label = QStringLiteral("0");
				if (frame > 0)
					label = QStringLiteral("+%1f").arg(frame);
				else if (frame < 0)
					label = QStringLiteral("%1f").arg(frame);
				painter.setPen(textColor);
				painter.drawText(QRectF(x - 24.0, axisY + 11.0, 48.0, 18.0),
						 Qt::AlignHCenter | Qt::AlignTop, label);
			}
		}
	}
	else {
		const qreal zeroX = valueToX(0.0);
		painter.setPen(QPen(textColor, 2.0));
		painter.drawLine(QPointF(zeroX, axisY - 9.0), QPointF(zeroX, axisY + 9.0));
		painter.drawText(QRectF(zeroX - 24.0, axisY + 11.0, 48.0, 18.0), Qt::AlignHCenter | Qt::AlignTop,
				 QStringLiteral("0"));
	}

	painter.setPen(textColor);
	painter.drawText(QRectF(left, 61.0, axisWidth / 2.0, 17.0), Qt::AlignLeft | Qt::AlignVCenter, audioEarlyLabel);
	painter.drawText(QRectF(left + axisWidth / 2.0, 61.0, axisWidth / 2.0, 17.0), Qt::AlignRight | Qt::AlignVCenter,
			 audioLateLabel);

	if (!hasMeasurement)
		return;

	const double rawMs = (double)offsetNs * 1e-6;
	const double markerMs = std::max(RULER_MIN_MS, std::min(RULER_MAX_MS, rawMs));
	const qreal markerX = valueToX(markerMs);

	QPolygonF marker;
	if (rawMs < RULER_MIN_MS) {
		marker << QPointF(left, axisY) << QPointF(left + 9.0, axisY - 7.0) << QPointF(left + 9.0, axisY + 7.0);
	}
	else if (rawMs > RULER_MAX_MS) {
		marker << QPointF(right, axisY) << QPointF(right - 9.0, axisY - 7.0)
		       << QPointF(right - 9.0, axisY + 7.0);
	}
	else {
		marker << QPointF(markerX, axisY + 1.0) << QPointF(markerX - 7.0, axisY - 11.0)
		       << QPointF(markerX + 7.0, axisY - 11.0);
	}
	painter.setPen(QPen(textColor, 1.0));
	painter.setBrush(markerColor);
	painter.drawPolygon(marker);

	QString rawLabel = QStringLiteral("%1 ms").arg(rawMs, 0, 'f', 1);
	if (rawMs < RULER_MIN_MS)
		rawLabel.prepend(QStringLiteral("< "));
	else if (rawMs > RULER_MAX_MS)
		rawLabel.prepend(QStringLiteral("> "));
	QFont markerFont = painter.font();
	markerFont.setWeight(QFont::DemiBold);
	painter.setFont(markerFont);
	const qreal labelWidth = std::max(74.0, (qreal)painter.fontMetrics().horizontalAdvance(rawLabel) + 8.0);
	const qreal labelLeft = std::max(left, std::min(right - labelWidth, markerX - labelWidth / 2.0));
	painter.setPen(markerColor);
	painter.drawText(QRectF(labelLeft, 2.0, labelWidth, 18.0), Qt::AlignHCenter | Qt::AlignVCenter, rawLabel);
}
