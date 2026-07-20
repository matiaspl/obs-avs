#pragma once
#include <QElapsedTimer>
#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <deque>
#include <string>
#include <vector>
#include <obs-frontend-api.h>
#include <obs.hpp>
#include "av-offset-ruler.hpp"
#include "sync-test-output.hpp"

class QTimer;

class SyncTestDock : public QFrame {
	Q_OBJECT

public:
	SyncTestDock(QWidget *parent = nullptr);
	~SyncTestDock();

private:
	enum class CorrectionStatusLevel { Info, Warning, Error, Success };

	QPushButton *startButton = nullptr;
	QPushButton *resetButton = nullptr;
	QPushButton *correctionButton = nullptr;

	QLabel *correctionStatus = nullptr;
	QLabel *latencyLabel = nullptr;
	QLabel *latencyDisplay = nullptr;
	AVOffsetRuler *latencyRuler = nullptr;
	QLabel *latencyPolarity = nullptr;
	QLabel *glassToGlassDisplay = nullptr;
	QLabel *indexDisplay = nullptr;
	QLabel *videoIndexDisplay = nullptr;
	QLabel *audioIndexDisplay = nullptr;

private:
	OBSOutput sync_test;

private:
	std::deque<int64_t> av_offset_samples;

	QTimer *correctionStateTimer = nullptr;
	bool frontendCallbackRegistered = false;
	QElapsedTimer correctionMeasurementAge;
	bool hasCorrectionMeasurement = false;
	int64_t correctionMeasurementNs = 0;
	std::string correctionSourceUuid;
	int64_t correctionSourceOffsetNs = 0;
	uint64_t transitionRevision = 0;
	uint64_t correctionTransitionRevision = 0;
	QString correctionWaitingStatus;
	CorrectionStatusLevel correctionWaitingLevel = CorrectionStatusLevel::Info;

	bool sampleContextInitialized = false;
	std::vector<std::string> sampleAudioTopology;
	bool sampleSourceOffsetValid = false;
	int64_t sampleSourceOffsetNs = 0;

private:
	void on_start_stop();
	void on_reset();
	void on_correct_sync_offset();
	void reset_analysis_stats();
	void update_correction_button_state();
	void set_correction_status(const QString &status, CorrectionStatusLevel level);
	void invalidate_correction_measurement(const QString &status = QString(),
					       CorrectionStatusLevel level = CorrectionStatusLevel::Info);
	void update_measurement_context();
	bool evaluate_correction(QString *status, std::string *source_uuid = nullptr, QString *source_name = nullptr,
				 int64_t *current_offset_ns = nullptr, int64_t *proposed_offset_ns = nullptr,
				 CorrectionStatusLevel *level = nullptr) const;
	void on_frontend_event(enum obs_frontend_event event);

	void on_video_marker_found(video_marker_found_s data);
	void on_audio_marker_found(audio_marker_found_s data);
	void on_sync_found(sync_index data);

	static void cb_video_marker_found(void *param, calldata_t *cd);
	static void cb_audio_marker_found(void *param, calldata_t *cd);
	static void cb_sync_found(void *param, calldata_t *cd);
	static void cb_frontend_event(enum obs_frontend_event event, void *param);
};
