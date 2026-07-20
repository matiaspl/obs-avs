/*
Klaps
Copyright (C) 2023 Norihiro Kamae <norihiro@nagater.net>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <obs-module.h>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QMessageBox>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
#include <obs-frontend-api.h>
#include "plugin-macros.generated.h"
#include "sync-test-dock.hpp"

#define ASSERT_THREAD(type)                                                                     \
	do {                                                                                    \
		if (!obs_in_task_thread(type))                                                  \
			blog(LOG_ERROR, "%s: ASSERT_THREAD failed: Expected " #type, __func__); \
	} while (false)

SyncTestDock::SyncTestDock(QWidget *parent) : QFrame(parent)
{
	QVBoxLayout *mainLayout = new QVBoxLayout();
	QGridLayout *topLayout = new QGridLayout();

	int y = 0;

	QHBoxLayout *buttonLayout = new QHBoxLayout();

	startButton = new QPushButton(obs_module_text("Button.Start"), this);
	buttonLayout->addWidget(startButton);
	connect(startButton, &QPushButton::clicked, this, &SyncTestDock::on_start_stop);

	resetButton = new QPushButton(obs_module_text("Button.Reset"), this);
	buttonLayout->addWidget(resetButton);
	connect(resetButton, &QPushButton::clicked, this, &SyncTestDock::on_reset);

	correctionButton = new QPushButton(obs_module_text("Button.CorrectSyncOffset"), this);
	correctionButton->setEnabled(false);
	buttonLayout->addWidget(correctionButton);
	connect(correctionButton, &QPushButton::clicked, this, &SyncTestDock::on_correct_sync_offset);

	mainLayout->addLayout(buttonLayout);

	correctionStatus = new QLabel(obs_module_text("Correction.Status.Waiting"), this);
	correctionStatus->setObjectName("correctionStatus");
	correctionStatus->setWordWrap(true);
	correctionStatus->setStyleSheet(
		"QLabel#correctionStatus { padding: 7px 9px; border: 1px solid rgba(105, 125, 145, 105); "
		"border-radius: 4px; background-color: rgba(105, 125, 145, 32); }"
		"QLabel#correctionStatus[statusLevel=\"warning\"] { border-color: rgba(230, 145, 35, 150); "
		"background-color: rgba(230, 145, 35, 48); }"
		"QLabel#correctionStatus[statusLevel=\"error\"] { border-color: rgba(210, 65, 55, 155); "
		"background-color: rgba(210, 65, 55, 48); }"
		"QLabel#correctionStatus[statusLevel=\"success\"] { border-color: rgba(45, 165, 85, 145); "
		"background-color: rgba(45, 165, 85, 45); }");
	correctionStatus->setProperty("statusLevel", "info");
	mainLayout->addWidget(correctionStatus);

	latencyRuler = new AVOffsetRuler(obs_module_text("Ruler.AudioEarly"), obs_module_text("Ruler.AudioLate"), this);
	latencyRuler->setObjectName("latencyRuler");
	topLayout->addWidget(latencyRuler, y++, 0, 1, 2);

	latencyLabel = new QLabel(obs_module_text("Label.AVOffset"), this);
	latencyLabel->setProperty("class", "text-large");
	topLayout->addWidget(latencyLabel, y, 0);

	latencyDisplay = new QLabel("-", this);
	latencyDisplay->setObjectName("latencyDisplay");
	latencyDisplay->setProperty("class", "text-large");
	topLayout->addWidget(latencyDisplay, y++, 1);

	QLabel *label = new QLabel(obs_module_text("Label.GlassToGlass"), this);
	topLayout->addWidget(label, y, 0);

	glassToGlassDisplay = new QLabel("-", this);
	glassToGlassDisplay->setObjectName("glassToGlassDisplay");
	glassToGlassDisplay->setProperty("class", "text-large");
	topLayout->addWidget(glassToGlassDisplay, y++, 1);

	label = new QLabel(obs_module_text("Label.Index"), this);
	topLayout->addWidget(label, y, 0);

	indexDisplay = new QLabel("-", this);
	indexDisplay->setObjectName("indexDisplay");
	topLayout->addWidget(indexDisplay, y++, 1);

	label = new QLabel(obs_module_text("Label.VideoIndex"), this);
	topLayout->addWidget(label, y, 0);

	videoIndexDisplay = new QLabel("-", this);
	videoIndexDisplay->setObjectName("videoIndexDisplay");
	topLayout->addWidget(videoIndexDisplay, y++, 1);

	label = new QLabel(obs_module_text("Label.AudioIndex"), this);
	topLayout->addWidget(label, y, 0);

	audioIndexDisplay = new QLabel("-", this);
	audioIndexDisplay->setObjectName("audioIndexDisplay");
	topLayout->addWidget(audioIndexDisplay, y++, 1);

	mainLayout->addLayout(topLayout);
	setLayout(mainLayout);
	reset_analysis_stats();

	obs_frontend_add_event_callback(cb_frontend_event, this);
	frontendCallbackRegistered = true;
	correctionStateTimer = new QTimer(this);
	correctionStateTimer->setInterval(500);
	connect(correctionStateTimer, &QTimer::timeout, this, &SyncTestDock::update_correction_button_state);
	correctionStateTimer->start();
}

SyncTestDock::~SyncTestDock()
{
	if (frontendCallbackRegistered)
		obs_frontend_remove_event_callback(cb_frontend_event, this);

	if (sync_test) {
		obs_output_stop(sync_test);
		sync_test = nullptr;
	}
}

extern "C" QWidget *create_sync_test_dock()
{
	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	return static_cast<QWidget *>(new SyncTestDock(main_window));
}

#define CD_TO_LOCAL(type, name, get_func) \
	type name;                        \
	if (!get_func(cd, #name, &name))  \
		return;

void SyncTestDock::cb_video_marker_found(void *param, calldata_t *cd)
{
	auto *dock = (SyncTestDock *)param;

	CD_TO_LOCAL(video_marker_found_s *, data, calldata_get_ptr);
	video_marker_found_s found = *data;

	QMetaObject::invokeMethod(dock, [dock, found]() { dock->on_video_marker_found(found); });
};

void SyncTestDock::cb_audio_marker_found(void *param, calldata_t *cd)
{
	auto *dock = (SyncTestDock *)param;

	CD_TO_LOCAL(audio_marker_found_s *, data, calldata_get_ptr);
	audio_marker_found_s found = *data;

	QMetaObject::invokeMethod(dock, [dock, found]() { dock->on_audio_marker_found(found); });
};

void SyncTestDock::cb_sync_found(void *param, calldata_t *cd)
{
	auto *dock = (SyncTestDock *)param;

	CD_TO_LOCAL(sync_index *, data, calldata_get_ptr);
	sync_index found = *data;

	QMetaObject::invokeMethod(dock, [dock, found]() { dock->on_sync_found(found); });
}

void SyncTestDock::cb_frontend_event(enum obs_frontend_event event, void *param)
{
	auto *dock = (SyncTestDock *)param;
	if (event == OBS_FRONTEND_EVENT_EXIT) {
		dock->frontendCallbackRegistered = false;
		return;
	}
	QMetaObject::invokeMethod(dock, [dock, event]() { dock->on_frontend_event(event); });
}

void SyncTestDock::on_start_stop()
{
	if (!sync_test) /* request to start */ {
		OBSOutputAutoRelease o = obs_output_create(OUTPUT_ID, "sync-test-output", nullptr, nullptr);
		if (!o) {
			blog(LOG_ERROR, "Failed to create sync-test-output.");
			return;
		}

		reset_analysis_stats();

		auto *sh = obs_output_get_signal_handler(o);
		signal_handler_connect(sh, "video_marker_found", cb_video_marker_found, this);
		signal_handler_connect(sh, "audio_marker_found", cb_audio_marker_found, this);
		signal_handler_connect(sh, "sync_found", cb_sync_found, this);

		bool success = obs_output_start(o);

		if (!success) {
			latencyDisplay->setText(obs_module_text("Display.Polarity.Failure"));
			blog(LOG_ERROR, "Failed to start sync-test-output.");
			return;
		}

		if (startButton)
			startButton->setText(obs_module_text("Button.Stop"));

		sync_test = o;
	}
	else /* request to stop */ {
		obs_output_stop(sync_test);
		sync_test = nullptr;

		if (startButton)
			startButton->setText(obs_module_text("Button.Start"));
	}
}

void SyncTestDock::on_reset()
{
	reset_analysis_stats();
}

struct active_audio_source
{
	std::string uuid;
	QString name;
	int64_t sync_offset_ns = 0;
};

static std::vector<active_audio_source> get_active_track1_audio_sources()
{
	std::vector<active_audio_source> sources;
	obs_enum_sources(
		[](void *param, obs_source_t *source) {
			auto *sources = static_cast<std::vector<active_audio_source> *>(param);
			const uint32_t flags = obs_source_get_output_flags(source);
			if (!(flags & OBS_SOURCE_AUDIO) || !obs_source_active(source) ||
			    !obs_source_audio_active(source) || !(obs_source_get_audio_mixers(source) & 1))
				return true;

			const char *uuid = obs_source_get_uuid(source);
			const char *name = obs_source_get_name(source);
			active_audio_source item;
			item.uuid = uuid ? uuid : "";
			item.name = QString::fromUtf8(name ? name : "");
			item.sync_offset_ns = obs_source_get_sync_offset(source);
			sources->push_back(item);
			return true;
		},
		&sources);

	std::sort(sources.begin(), sources.end(),
		  [](const active_audio_source &a, const active_audio_source &b) { return a.uuid < b.uuid; });
	return sources;
}

static std::vector<std::string> audio_source_topology(const std::vector<active_audio_source> &sources)
{
	std::vector<std::string> topology;
	topology.reserve(sources.size());
	for (const auto &source : sources)
		topology.push_back(source.uuid);
	return topology;
}

static bool program_transition_active()
{
	OBSSourceAutoRelease transition = obs_frontend_get_current_transition();
	if (!transition)
		return false;

	const float progress = obs_transition_get_time(transition);
	return progress > 0.0f && progress < 1.0f;
}

static constexpr int64_t NS_PER_MS = 1000000;
static constexpr int64_t MIN_SYNC_OFFSET_MS = -950;
static constexpr int64_t MAX_SYNC_OFFSET_MS = 20000;
static constexpr qint64 CORRECTION_FRESHNESS_MS = 10000;

void SyncTestDock::invalidate_correction_measurement(const QString &status, CorrectionStatusLevel level)
{
	hasCorrectionMeasurement = false;
	correctionMeasurementAge.invalidate();
	correctionSourceUuid.clear();
	correctionSourceOffsetNs = 0;
	correctionWaitingStatus = status;
	correctionWaitingLevel = level;
}

void SyncTestDock::update_measurement_context()
{
	const std::vector<active_audio_source> sources = get_active_track1_audio_sources();
	const std::vector<std::string> topology = audio_source_topology(sources);
	bool context_changed = sampleContextInitialized && topology != sampleAudioTopology;

	if (sources.size() == 1) {
		if (sampleContextInitialized && sampleSourceOffsetValid &&
		    sources[0].sync_offset_ns != sampleSourceOffsetNs)
			context_changed = true;
		sampleSourceOffsetValid = true;
		sampleSourceOffsetNs = sources[0].sync_offset_ns;
	}
	else {
		sampleSourceOffsetValid = false;
		sampleSourceOffsetNs = 0;
	}

	if (context_changed) {
		av_offset_samples.clear();
		invalidate_correction_measurement();
	}

	sampleContextInitialized = true;
	sampleAudioTopology = topology;
}

bool SyncTestDock::evaluate_correction(QString *status, std::string *source_uuid, QString *source_name,
				       int64_t *current_offset_ns, int64_t *proposed_offset_ns,
				       CorrectionStatusLevel *level) const
{
	const auto set_level = [level](CorrectionStatusLevel value) {
		if (level)
			*level = value;
	};

	if (program_transition_active()) {
		*status = obs_module_text("Correction.Status.TransitionActive");
		set_level(CorrectionStatusLevel::Warning);
		return false;
	}

	const std::vector<active_audio_source> sources = get_active_track1_audio_sources();
	if (sources.empty()) {
		*status = obs_module_text("Correction.Status.NoAudioSource");
		set_level(CorrectionStatusLevel::Warning);
		return false;
	}
	if (sources.size() != 1) {
		*status = QString::fromUtf8(obs_module_text("Correction.Status.MultipleAudioSources"))
				  .arg((qulonglong)sources.size());
		set_level(CorrectionStatusLevel::Warning);
		return false;
	}
	if (sources[0].uuid.empty()) {
		*status = obs_module_text("Correction.Status.UnidentifiedSource");
		set_level(CorrectionStatusLevel::Error);
		return false;
	}

	if (!hasCorrectionMeasurement) {
		*status = correctionWaitingStatus.isEmpty() ? obs_module_text("Correction.Status.Waiting")
							    : correctionWaitingStatus;
		set_level(correctionWaitingLevel);
		return false;
	}
	if (correctionTransitionRevision != transitionRevision) {
		*status = obs_module_text("Correction.Status.TransitionOccurred");
		set_level(CorrectionStatusLevel::Warning);
		return false;
	}
	if (!correctionMeasurementAge.isValid() || correctionMeasurementAge.elapsed() > CORRECTION_FRESHNESS_MS) {
		*status = obs_module_text("Correction.Status.Expired");
		set_level(CorrectionStatusLevel::Warning);
		return false;
	}
	if (sources[0].uuid != correctionSourceUuid) {
		*status = obs_module_text("Correction.Status.SourceChanged");
		set_level(CorrectionStatusLevel::Warning);
		return false;
	}
	if (sources[0].sync_offset_ns != correctionSourceOffsetNs) {
		*status = obs_module_text("Correction.Status.OffsetChanged");
		set_level(CorrectionStatusLevel::Warning);
		return false;
	}

	const int64_t proposed_ms =
		(int64_t)std::llround((double)(sources[0].sync_offset_ns - correctionMeasurementNs) / NS_PER_MS);
	if (proposed_ms < MIN_SYNC_OFFSET_MS || proposed_ms > MAX_SYNC_OFFSET_MS) {
		*status = QString::fromUtf8(obs_module_text("Correction.Status.OutOfRange")).arg(proposed_ms);
		set_level(CorrectionStatusLevel::Error);
		return false;
	}

	const int64_t proposed_ns = proposed_ms * NS_PER_MS;
	*status = QString::fromUtf8(obs_module_text("Correction.Status.Ready"))
			  .arg(sources[0].name)
			  .arg((double)sources[0].sync_offset_ns / NS_PER_MS, 0, 'f', 0)
			  .arg(proposed_ms);
	if (source_uuid)
		*source_uuid = sources[0].uuid;
	if (source_name)
		*source_name = sources[0].name;
	if (current_offset_ns)
		*current_offset_ns = sources[0].sync_offset_ns;
	if (proposed_offset_ns)
		*proposed_offset_ns = proposed_ns;
	set_level(CorrectionStatusLevel::Success);
	return true;
}

void SyncTestDock::set_correction_status(const QString &status, CorrectionStatusLevel level)
{
	const char *status_level = "info";
	switch (level) {
	case CorrectionStatusLevel::Warning:
		status_level = "warning";
		break;
	case CorrectionStatusLevel::Error:
		status_level = "error";
		break;
	case CorrectionStatusLevel::Success:
		status_level = "success";
		break;
	case CorrectionStatusLevel::Info:
		break;
	}

	correctionStatus->setText(status);
	if (correctionStatus->property("statusLevel").toByteArray() == status_level)
		return;
	correctionStatus->setProperty("statusLevel", status_level);
	correctionStatus->style()->unpolish(correctionStatus);
	correctionStatus->style()->polish(correctionStatus);
	correctionStatus->update();
}

void SyncTestDock::update_correction_button_state()
{
	if (!correctionButton || !correctionStatus)
		return;

	QString status;
	CorrectionStatusLevel level = CorrectionStatusLevel::Info;
	const bool enabled = evaluate_correction(&status, nullptr, nullptr, nullptr, nullptr, &level);
	correctionButton->setEnabled(enabled);
	set_correction_status(status, level);
}

void SyncTestDock::on_correct_sync_offset()
{
	QString status;
	std::string source_uuid;
	QString source_name;
	int64_t current_offset_ns = 0;
	int64_t proposed_offset_ns = 0;
	if (!evaluate_correction(&status, &source_uuid, &source_name, &current_offset_ns, &proposed_offset_ns)) {
		update_correction_button_state();
		return;
	}

	const QString confirmation = QString::fromUtf8(obs_module_text("Correction.Confirm.Text"))
					     .arg(source_name)
					     .arg((double)current_offset_ns / NS_PER_MS, 0, 'f', 0)
					     .arg((double)proposed_offset_ns / NS_PER_MS, 0, 'f', 0)
					     .arg((double)correctionMeasurementNs / NS_PER_MS, 0, 'f', 1);
	if (QMessageBox::question(this, obs_module_text("Correction.Confirm.Title"), confirmation,
				  QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
		return;

	std::string revalidated_uuid;
	int64_t revalidated_current_ns = 0;
	int64_t revalidated_proposed_ns = 0;
	if (!evaluate_correction(&status, &revalidated_uuid, nullptr, &revalidated_current_ns,
				 &revalidated_proposed_ns) ||
	    revalidated_uuid != source_uuid || revalidated_current_ns != current_offset_ns ||
	    revalidated_proposed_ns != proposed_offset_ns) {
		QMessageBox::warning(this, obs_module_text("Correction.Aborted.Title"),
				     QString::fromUtf8(obs_module_text("Correction.Aborted.Text")).arg(status));
		update_correction_button_state();
		return;
	}

	OBSSourceAutoRelease source = obs_get_source_by_uuid(source_uuid.c_str());
	if (!source) {
		QMessageBox::warning(this, obs_module_text("Correction.Aborted.Title"),
				     obs_module_text("Correction.Status.SourceChanged"));
		update_correction_button_state();
		return;
	}

	obs_source_set_sync_offset(source, proposed_offset_ns);
	obs_frontend_save();
	blog(LOG_INFO, "Set Sync Offset for '%s' from %.0f ms to %.0f ms using measured AV offset %.1f ms",
	     source_name.toUtf8().constData(), (double)current_offset_ns / NS_PER_MS,
	     (double)proposed_offset_ns / NS_PER_MS, (double)correctionMeasurementNs / NS_PER_MS);

	const QString applied_status = QString::fromUtf8(obs_module_text("Correction.Status.Applied"))
					       .arg(source_name)
					       .arg((double)proposed_offset_ns / NS_PER_MS, 0, 'f', 0);
	reset_analysis_stats();
	correctionWaitingStatus = applied_status;
	correctionWaitingLevel = CorrectionStatusLevel::Success;
	update_correction_button_state();
}

void SyncTestDock::on_frontend_event(enum obs_frontend_event event)
{
	if (event != OBS_FRONTEND_EVENT_TRANSITION_STOPPED && event != OBS_FRONTEND_EVENT_SCENE_CHANGED)
		return;

	transitionRevision++;
	av_offset_samples.clear();
	sampleContextInitialized = false;
	sampleAudioTopology.clear();
	sampleSourceOffsetValid = false;
	invalidate_correction_measurement(obs_module_text("Correction.Status.TransitionOccurred"),
					  CorrectionStatusLevel::Warning);
	update_correction_button_state();
}

void SyncTestDock::reset_analysis_stats()
{
	av_offset_samples.clear();
	sampleContextInitialized = false;
	sampleAudioTopology.clear();
	sampleSourceOffsetValid = false;
	sampleSourceOffsetNs = 0;
	invalidate_correction_measurement();

	if (latencyLabel)
		latencyLabel->setText(obs_module_text("Label.AVOffset"));
	if (latencyDisplay)
		latencyDisplay->setText(QStringLiteral("-"));
	if (latencyRuler)
		latencyRuler->clearMeasurement();
	if (glassToGlassDisplay)
		glassToGlassDisplay->setText(QStringLiteral("-"));
	if (indexDisplay)
		indexDisplay->setText(QStringLiteral("-"));
	if (videoIndexDisplay)
		videoIndexDisplay->setText(QStringLiteral("-"));
	if (audioIndexDisplay)
		audioIndexDisplay->setText(QStringLiteral("-"));
	update_correction_button_state();
}

static constexpr int64_t AV_OFFSET_PHASE_RESET_NS = 15000000;

static int64_t median_ns(const std::deque<int64_t> &samples)
{
	std::vector<int64_t> sorted(samples.begin(), samples.end());
	std::sort(sorted.begin(), sorted.end());
	const size_t mid = sorted.size() / 2;
	if (sorted.size() & 1)
		return sorted[mid];
	return (sorted[mid - 1] + sorted[mid]) / 2;
}

static int64_t median_abs_deviation_ns(const std::deque<int64_t> &samples, int64_t median)
{
	std::deque<int64_t> deviations;
	for (int64_t sample : samples)
		deviations.push_back(std::llabs(sample - median));
	return median_ns(deviations);
}

static double ns_to_ms(int64_t ns)
{
	return (double)ns * 1e-6;
}

void SyncTestDock::on_video_marker_found(struct video_marker_found_s data)
{
	const uint64_t display_sequence = data.qr_data.has_ntp_ms ? data.qr_data.index : data.sequence;
	videoIndexDisplay->setText(QStringLiteral("%1").arg(display_sequence));
	if (glassToGlassDisplay) {
		if (data.has_glass_to_glass)
			glassToGlassDisplay->setText(
				QStringLiteral("%1 ms").arg(ns_to_ms(data.glass_to_glass_ns), 0, 'f', 1));
		else
			glassToGlassDisplay->setText(QStringLiteral("-"));
	}
}

void SyncTestDock::on_audio_marker_found(struct audio_marker_found_s data)
{
	audioIndexDisplay->setText(QStringLiteral("%1").arg(data.sequence));
}

void SyncTestDock::on_sync_found(sync_index data)
{
	int64_t ts = (int64_t)data.audio_ts - (int64_t)data.video_ts;
	int64_t display_ts = ts;
	update_measurement_context();
	if (data.has_glass_to_glass && glassToGlassDisplay)
		glassToGlassDisplay->setText(QStringLiteral("%1 ms").arg(ns_to_ms(data.glass_to_glass_ns), 0, 'f', 1));
	if (!av_offset_samples.empty()) {
		const int64_t median = median_ns(av_offset_samples);
		if (std::llabs(ts - median) > AV_OFFSET_PHASE_RESET_NS) {
			av_offset_samples.clear();
			invalidate_correction_measurement();
		}
	}
	av_offset_samples.push_back(ts);
	while (av_offset_samples.size() > 9)
		av_offset_samples.pop_front();
	if (av_offset_samples.size() >= 3) {
		display_ts = median_ns(av_offset_samples);
		const int64_t jitter = median_abs_deviation_ns(av_offset_samples, display_ts);
		latencyDisplay->setText(QStringLiteral("%1 ms median, +/- %2 ms")
						.arg(ns_to_ms(display_ts), 0, 'f', 1)
						.arg(ns_to_ms(jitter), 0, 'f', 1));

		hasCorrectionMeasurement = true;
		correctionMeasurementNs = display_ts;
		correctionTransitionRevision = transitionRevision;
		correctionMeasurementAge.restart();
		correctionWaitingStatus.clear();
		if (sampleAudioTopology.size() == 1 && sampleSourceOffsetValid) {
			correctionSourceUuid = sampleAudioTopology[0];
			correctionSourceOffsetNs = sampleSourceOffsetNs;
		}
		else {
			correctionSourceUuid.clear();
			correctionSourceOffsetNs = 0;
		}
	}
	else {
		latencyDisplay->setText(QStringLiteral("%1 ms").arg(ns_to_ms(ts), 0, 'f', 1));
	}
	struct obs_video_info video_info = {};
	const bool has_video_info = obs_get_video_info(&video_info);
	if (latencyRuler)
		latencyRuler->setMeasurement(ts, has_video_info ? video_info.fps_num : 0,
					     has_video_info ? video_info.fps_den : 0);
	indexDisplay->setText(QStringLiteral("%1").arg(data.sequence));
	update_correction_button_state();
}
