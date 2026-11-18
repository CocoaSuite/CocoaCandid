#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QVector>
#include <obs.hpp>

struct EncoderChoice {
	QString id;
	QString name;
	bool hardware = false;
};

struct OutputStats {
	double cpu = 0;
	double fps = 0;
	double renderMs = 0;
	uint32_t laggedFrames = 0, totalFrames = 0;
	int droppedStream = 0, totalStream = 0;
	double streamKbps = 0;
	double recordMb = 0;
};

/**
 * Recording, streaming, the replay buffer and the virtual camera. Encoders are made fresh each time an
 * output starts, from the settings in Config, so a change in Settings applies to the next start.
 */
class Outputs : public QObject {
	Q_OBJECT
public:
	static Outputs &get();

	bool recording() const { return m_recording; }
	bool paused() const { return m_paused; }
	bool streaming() const { return m_streaming; }
	bool streamStarting() const { return m_streamStarting; }
	bool reconnecting() const { return m_reconnecting; }
	bool replayActive() const { return m_replay; }
	bool virtualCamActive() const { return m_vcam; }
	bool virtualCamAvailable() const;
	bool anyActive() const { return m_recording || m_streaming || m_replay || m_vcam; }

	bool startRecording(QString *error);
	void stopRecording();
	bool setPaused(bool on);
	bool startStreaming(QString *error);
	void stopStreaming();
	bool startReplay(QString *error);
	void stopReplay();
	void saveReplay();
	bool startVirtualCam(QString *error);
	void stopVirtualCam();
	void stopAll();

	/** Seconds since the recording or stream started, not counting pauses. */
	qint64 recordSeconds() const;
	qint64 streamSeconds() const;
	QString lastRecording() const { return m_lastFile; }

	OutputStats stats();

	/** Video encoders a loaded plugin provides, hardware first. */
	static QVector<EncoderChoice> videoEncoders();
	/** The encoder to use when none is chosen: NVIDIA, then Intel, then AMD, then x264. */
	static QString bestEncoder();
	/** Settings for a video encoder from a stream bitrate or a recording quality. */
	static obs_data_t *videoSettings(const QString &id, bool forStream);

signals:
	void stateChanged();
	void error(const QString &message);
	void replaySaved(const QString &path);
	void recordingSaved(const QString &path);

private:
	Outputs() = default;
	OBSEncoder makeVideo(const char *name, bool forStream);
	OBSEncoder makeAudio(const char *name, int kbps, size_t mixer);
	void watch(obs_output_t *out, const char *kind);
	static void onStart(void *data, calldata_t *cd);
	static void onStop(void *data, calldata_t *cd);
	static void onReconnect(void *data, calldata_t *cd);
	static void onReconnected(void *data, calldata_t *cd);
	static void onReplaySaved(void *data, calldata_t *cd);
	void handleStop(obs_output_t *out, int code);

	OBSOutput m_record, m_stream, m_replayOut, m_vcamOut;
	OBSService m_service;
	bool m_recording = false, m_paused = false, m_streaming = false, m_streamStarting = false, m_reconnecting = false;
	bool m_replay = false, m_vcam = false;
	QElapsedTimer m_recClock, m_streamClock;
	qint64 m_pausedMs = 0, m_pauseStart = 0;
	QString m_lastFile;
	uint64_t m_lastBytes = 0;
	qint64 m_lastBytesAt = 0;
	double m_kbps = 0;
	void *m_cpu = nullptr;
};

#define OUTPUTS Outputs::get()
