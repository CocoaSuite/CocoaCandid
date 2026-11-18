#include "Outputs.hpp"
#include "Config.hpp"
#include "Engine.hpp"

#include <QDateTime>
#include <QDir>
#include <QMetaObject>
#include <util/platform.h>

Outputs &Outputs::get()
{
	static Outputs o;
	return o;
}

static bool hasOutput(const char *id)
{
	return obs_initialized() && obs_output_get_display_name(id) != nullptr;
}

// Encoders --------------------------------------------------------------------------------------------

static bool isHardware(const QString &id)
{
	return id.contains("nvenc") || id.contains("qsv") || id.contains("amf") || id.contains("vaapi") ||
	       id.contains("videotoolbox") || id.contains("_mf");
}

QVector<EncoderChoice> Outputs::videoEncoders()
{
	QVector<EncoderChoice> hw, sw;
	const char *id;
	for (size_t i = 0; obs_enum_encoder_types(i, &id); i++) {
		if (obs_get_encoder_type(id) != OBS_ENCODER_VIDEO)
			continue;
		const uint32_t caps = obs_get_encoder_caps(id);
		if (caps & (OBS_ENCODER_CAP_DEPRECATED | OBS_ENCODER_CAP_INTERNAL))
			continue;
		EncoderChoice c;
		c.id = QString::fromUtf8(id);
		c.name = QString::fromUtf8(obs_encoder_get_display_name(id));
		c.hardware = isHardware(c.id) || (caps & OBS_ENCODER_CAP_PASS_TEXTURE);
		(c.hardware ? hw : sw).append(c);
	}
	return hw + sw;
}

QString Outputs::bestEncoder()
{
	const auto list = videoEncoders();
	auto has = [&](const char *id) {
		for (const auto &e : list)
			if (e.id == id)
				return true;
		return false;
	};
	for (const char *id : {"obs_nvenc_h264_tex", "jim_nvenc", "obs_qsv11_v2", "obs_qsv11", "h264_texture_amf",
			       "ffmpeg_vaapi_tex", "obs_x264"})
		if (has(id))
			return id;
	return list.isEmpty() ? QString() : list.first().id;
}

/** Sets a key only where the encoder has it, so one recipe works for every encoder. */
static void setIfKnown(obs_data_t *defaults, obs_data_t *s, const char *key, long long v)
{
	if (obs_data_has_default_value(defaults, key) || obs_data_has_user_value(defaults, key))
		obs_data_set_int(s, key, v);
}

obs_data_t *Outputs::videoSettings(const QString &id, bool forStream)
{
	const QByteArray idb = id.toUtf8();
	obs_data_t *s = obs_encoder_defaults(idb.constData());
	if (!s)
		s = obs_data_create();
	OBSDataAutoRelease defs = obs_encoder_defaults(idb.constData());
	const bool x264 = id.contains("x264");
	obs_data_set_int(s, "keyint_sec", CFG.num("stream", "keyint_sec"));

	QString quality = forStream ? QString("stream") : CFG.str("record", "quality");
	if (quality == "same")
		quality = "stream";

	if (quality == "stream") {
		obs_data_set_string(s, "rate_control", "CBR");
		obs_data_set_int(s, "bitrate", CFG.num("stream", "bitrate"));
	} else if (quality == "lossless") {
		if (id.contains("nvenc")) {
			obs_data_set_string(s, "rate_control", "lossless");
		} else {
			obs_data_set_string(s, "rate_control", x264 ? "CRF" : "CQP");
			obs_data_set_int(s, "crf", 0);
			setIfKnown(defs, s, "cqp", 1);
			setIfKnown(defs, s, "qp", 1);
		}
	} else {
		const int q = quality == "indistinguishable" ? 16 : 21;
		if (id.contains("qsv")) {
			obs_data_set_string(s, "rate_control", "ICQ");
			obs_data_set_int(s, "icq_quality", q + 2);
		} else {
			obs_data_set_string(s, "rate_control", x264 ? "CRF" : "CQP");
		}
		obs_data_set_int(s, "crf", q + 1);
		setIfKnown(defs, s, "cqp", q);
		setIfKnown(defs, s, "qp", q);
		setIfKnown(defs, s, "qpi", q);
		setIfKnown(defs, s, "qpp", q);
		setIfKnown(defs, s, "qpb", q);
		obs_data_set_int(s, "bitrate", 40000);
	}
	if (x264)
		obs_data_set_string(s, "preset", forStream ? "veryfast" : "veryfast");

	// Anything changed by hand in Settings, Advanced, wins.
	OBSDataAutoRelease sec = obs_data_get_obj(CFG.data(), forStream ? "stream" : "record");
	OBSDataAutoRelease custom = sec ? obs_data_get_obj(sec, "encoder_settings") : nullptr;
	if (custom && strcmp(obs_data_get_string(sec, "encoder_settings_for"), idb.constData()) == 0)
		obs_data_apply(s, custom);
	return s;
}

OBSEncoder Outputs::makeVideo(const char *name, bool forStream)
{
	QString id = CFG.str(forStream ? "stream" : "record", "encoder");
	if (id.isEmpty() || !obs_encoder_get_display_name(id.toUtf8().constData()))
		id = bestEncoder();
	OBSDataAutoRelease s = videoSettings(id, forStream);
	if (forStream && m_service)
		obs_service_apply_encoder_settings(m_service, s, nullptr);
	OBSEncoderAutoRelease e = obs_video_encoder_create(id.toUtf8().constData(), name, s, nullptr);
	if (!e && id != "obs_x264")
		e = obs_video_encoder_create("obs_x264", name, s, nullptr);
	if (e)
		obs_encoder_set_video(e, obs_get_video());
	return OBSEncoder(e.Get());
}

OBSEncoder Outputs::makeAudio(const char *name, int kbps, size_t mixer)
{
	OBSDataAutoRelease s = obs_data_create();
	obs_data_set_int(s, "bitrate", kbps);
	OBSEncoderAutoRelease e = obs_audio_encoder_create("ffmpeg_aac", name, s, mixer, nullptr);
	if (e)
		obs_encoder_set_audio(e, obs_get_audio());
	return OBSEncoder(e.Get());
}

// Signals ---------------------------------------------------------------------------------------------

void Outputs::watch(obs_output_t *out, const char *)
{
	signal_handler_t *sh = obs_output_get_signal_handler(out);
	signal_handler_connect(sh, "start", onStart, this);
	signal_handler_connect(sh, "stop", onStop, this);
	signal_handler_connect(sh, "reconnect", onReconnect, this);
	signal_handler_connect(sh, "reconnect_success", onReconnected, this);
}

void Outputs::onStart(void *data, calldata_t *cd)
{
	auto *self = static_cast<Outputs *>(data);
	obs_output_t *out = static_cast<obs_output_t *>(calldata_ptr(cd, "output"));
	QMetaObject::invokeMethod(self, [self, out] {
		if (out == self->m_stream) {
			self->m_streamStarting = false;
			self->m_streaming = true;
			self->m_streamClock.start();
		}
		emit self->stateChanged();
	}, Qt::QueuedConnection);
}

void Outputs::onStop(void *data, calldata_t *cd)
{
	auto *self = static_cast<Outputs *>(data);
	obs_output_t *out = static_cast<obs_output_t *>(calldata_ptr(cd, "output"));
	const int code = int(calldata_int(cd, "code"));
	OBSOutput keep(out);
	QMetaObject::invokeMethod(self, [self, keep, code] { self->handleStop(keep, code); }, Qt::QueuedConnection);
}

void Outputs::onReconnect(void *data, calldata_t *)
{
	auto *self = static_cast<Outputs *>(data);
	QMetaObject::invokeMethod(self, [self] {
		self->m_reconnecting = true;
		emit self->stateChanged();
	}, Qt::QueuedConnection);
}

void Outputs::onReconnected(void *data, calldata_t *)
{
	auto *self = static_cast<Outputs *>(data);
	QMetaObject::invokeMethod(self, [self] {
		self->m_reconnecting = false;
		emit self->stateChanged();
	}, Qt::QueuedConnection);
}

void Outputs::onReplaySaved(void *data, calldata_t *)
{
	auto *self = static_cast<Outputs *>(data);
	QMetaObject::invokeMethod(self, [self] {
		if (!self->m_replayOut)
			return;
		calldata_t cd = {};
		proc_handler_call(obs_output_get_proc_handler(self->m_replayOut), "get_last_replay", &cd);
		const QString path = QString::fromUtf8(calldata_string(&cd, "path"));
		calldata_free(&cd);
		emit self->replaySaved(path);
	}, Qt::QueuedConnection);
}

static QString stopMessage(obs_output_t *out, int code)
{
	const char *last = obs_output_get_last_error(out);
	QString detail = last && *last ? QString::fromUtf8(last) : QString();
	switch (code) {
	case OBS_OUTPUT_SUCCESS:
		return {};
	case OBS_OUTPUT_BAD_PATH:
		return QObject::tr("The recording folder can't be written to. Pick another in Settings, Record.");
	case OBS_OUTPUT_CONNECT_FAILED:
		return QObject::tr("Could not connect to the server. Check the internet connection and the server in Settings, Stream.");
	case OBS_OUTPUT_INVALID_STREAM:
		return QObject::tr("The server refused the stream key. Paste it again in Settings, Stream.");
	case OBS_OUTPUT_DISCONNECTED:
		return QObject::tr("The connection to the server was lost.");
	case OBS_OUTPUT_NO_SPACE:
		return QObject::tr("The disk is full.");
	case OBS_OUTPUT_ENCODE_ERROR:
		return QObject::tr("The video encoder stopped working. Try another encoder in Settings.") +
		       (detail.isEmpty() ? QString() : "\n\n" + detail);
	case OBS_OUTPUT_UNSUPPORTED:
		return QObject::tr("This encoder or format isn't supported here.");
	default:
		return detail.isEmpty() ? QObject::tr("Something went wrong (code %1).").arg(code) : detail;
	}
}

void Outputs::handleStop(obs_output_t *out, int code)
{
	const QString msg = stopMessage(out, code);
	if (out == m_record) {
		m_recording = false;
		m_paused = false;
		OBSDataAutoRelease s = obs_output_get_settings(out);
		m_lastFile = QString::fromUtf8(obs_data_get_string(s, "path"));
		m_record = nullptr;
		if (code == OBS_OUTPUT_SUCCESS)
			emit recordingSaved(m_lastFile);
	} else if (out == m_stream) {
		m_streaming = false;
		m_streamStarting = false;
		m_reconnecting = false;
		m_stream = nullptr;
		m_service = nullptr;
	} else if (out == m_replayOut) {
		m_replay = false;
		m_replayOut = nullptr;
	} else if (out == m_vcamOut) {
		m_vcam = false;
		m_vcamOut = nullptr;
	}
	if (!msg.isEmpty())
		emit error(msg);
	emit stateChanged();
}

// Recording -------------------------------------------------------------------------------------------

static QString recordPath(const QString &ext, const QString &suffix = {})
{
	QString dir = CFG.str("record", "path");
	if (dir.isEmpty())
		dir = QDir::homePath();
	QDir().mkpath(dir);
	const QString name = QDateTime::currentDateTime().toString("yyyy-MM-dd HH-mm-ss") + suffix + "." + ext;
	return QDir(dir).filePath(name);
}

bool Outputs::startRecording(QString *error)
{
	if (m_recording)
		return true;
	const QString fmt = CFG.str("record", "format");
	const bool hybrid = fmt == "mp4" && hasOutput("mp4_output");
	OBSDataAutoRelease s = obs_data_create();
	obs_data_set_string(s, "path", QDir::toNativeSeparators(recordPath(fmt)).toUtf8().constData());
	if (fmt == "mp4" && !hybrid)
		obs_data_set_string(s, "muxer_settings", "movflags=frag_keyframe+empty_moov+delay_moov");
	OBSOutputAutoRelease out = obs_output_create(hybrid ? "mp4_output" : "ffmpeg_muxer", "candid_record", s, nullptr);
	if (!out) {
		if (error)
			*error = tr("Recording isn't available: the output plugin didn't load.");
		return false;
	}
	OBSEncoder v = makeVideo("candid_record_video", false);
	OBSEncoder a = makeAudio("candid_record_audio", int(CFG.num("record", "audio_bitrate")), 0);
	if (!v || !a) {
		if (error)
			*error = tr("No video encoder could be started.");
		return false;
	}
	obs_output_set_video_encoder(out, v);
	obs_output_set_audio_encoder(out, a, 0);
	watch(out, "record");
	m_record = out.Get();
	if (!obs_output_start(out)) {
		if (error)
			*error = stopMessage(out, OBS_OUTPUT_ERROR);
		m_record = nullptr;
		return false;
	}
	m_recording = true;
	m_paused = false;
	m_pausedMs = 0;
	m_recClock.start();
	emit stateChanged();
	return true;
}

void Outputs::stopRecording()
{
	if (m_record)
		obs_output_stop(m_record);
}

bool Outputs::setPaused(bool on)
{
	if (!m_record || on == m_paused || !obs_output_can_pause(m_record))
		return false;
	if (!obs_output_pause(m_record, on))
		return false;
	m_paused = on;
	if (on)
		m_pauseStart = m_recClock.elapsed();
	else
		m_pausedMs += m_recClock.elapsed() - m_pauseStart;
	emit stateChanged();
	return true;
}

qint64 Outputs::recordSeconds() const
{
	if (!m_recording)
		return 0;
	qint64 ms = m_recClock.elapsed() - m_pausedMs;
	if (m_paused)
		ms -= m_recClock.elapsed() - m_pauseStart;
	return ms / 1000;
}

qint64 Outputs::streamSeconds() const
{
	return m_streaming ? m_streamClock.elapsed() / 1000 : 0;
}

// Streaming -------------------------------------------------------------------------------------------

bool Outputs::startStreaming(QString *error)
{
	if (m_streaming || m_streamStarting)
		return true;
	const QString type = CFG.str("stream", "type");
	const QString key = CFG.str("stream", "key").trimmed();
	OBSDataAutoRelease ss = obs_data_create();
	if (type == "rtmp_custom" || type == "whip_custom") {
		const QString server = CFG.str("stream", "custom_server").trimmed();
		if (server.isEmpty()) {
			if (error)
				*error = tr("Add the server address in Settings, Stream.");
			return false;
		}
		obs_data_set_string(ss, "server", server.toUtf8().constData());
		obs_data_set_string(ss, type == "whip_custom" ? "bearer_token" : "key", key.toUtf8().constData());
	} else {
		if (key.isEmpty()) {
			if (error)
				*error = tr("Add your stream key in Settings, Stream.");
			return false;
		}
		obs_data_set_string(ss, "service", CFG.str("stream", "service").toUtf8().constData());
		obs_data_set_string(ss, "server", CFG.str("stream", "server").toUtf8().constData());
		obs_data_set_string(ss, "key", key.toUtf8().constData());
	}
	OBSServiceAutoRelease svc = obs_service_create(type.toUtf8().constData(), "candid_service", ss, nullptr);
	if (!svc) {
		if (error)
			*error = tr("Streaming isn't available: the services plugin didn't load.");
		return false;
	}
	m_service = svc.Get();

	const char *outType = obs_service_get_preferred_output_type(svc);
	if (!outType || !hasOutput(outType))
		outType = "rtmp_output";
	OBSDataAutoRelease os = obs_data_create();
	obs_data_set_bool(os, "dyn_bitrate", CFG.flag("stream", "dyn_bitrate"));
	OBSOutputAutoRelease out = obs_output_create(outType, "candid_stream", os, nullptr);
	if (!out) {
		m_service = nullptr;
		if (error)
			*error = tr("Streaming isn't available: the output plugin didn't load.");
		return false;
	}
	OBSEncoder v = makeVideo("candid_stream_video", true);
	OBSEncoder a = makeAudio("candid_stream_audio", int(CFG.num("stream", "audio_bitrate")), 0);
	if (!v || !a) {
		m_service = nullptr;
		if (error)
			*error = tr("No video encoder could be started.");
		return false;
	}
	obs_output_set_video_encoder(out, v);
	obs_output_set_audio_encoder(out, a, 0);
	obs_output_set_service(out, svc);
	obs_output_set_reconnect_settings(out, 20, 3);
	watch(out, "stream");
	m_stream = out.Get();
	m_streamStarting = true;
	emit stateChanged();
	if (!obs_output_start(out)) {
		if (error)
			*error = stopMessage(out, OBS_OUTPUT_CONNECT_FAILED);
		m_stream = nullptr;
		m_service = nullptr;
		m_streamStarting = false;
		emit stateChanged();
		return false;
	}
	m_lastBytes = 0;
	m_lastBytesAt = 0;
	return true;
}

void Outputs::stopStreaming()
{
	if (m_stream)
		obs_output_stop(m_stream);
}

// Replay buffer ---------------------------------------------------------------------------------------

bool Outputs::startReplay(QString *error)
{
	if (m_replay)
		return true;
	if (!hasOutput("replay_buffer")) {
		if (error)
			*error = tr("The replay buffer isn't available.");
		return false;
	}
	QString dir = CFG.str("record", "path");
	QDir().mkpath(dir);
	const QString fmt = CFG.str("record", "format");
	OBSDataAutoRelease s = obs_data_create();
	obs_data_set_string(s, "directory", QDir::toNativeSeparators(dir).toUtf8().constData());
	obs_data_set_string(s, "format", "%CCYY-%MM-%DD %hh-%mm-%ss Replay");
	obs_data_set_string(s, "extension", (fmt == "mkv" || fmt == "mov") ? fmt.toUtf8().constData() : "mp4");
	obs_data_set_int(s, "max_time_sec", CFG.num("record", "replay_sec"));
	obs_data_set_int(s, "max_size_mb", 4096);
	OBSOutputAutoRelease out = obs_output_create("replay_buffer", "candid_replay", s, nullptr);
	OBSEncoder v = makeVideo("candid_replay_video", false);
	OBSEncoder a = makeAudio("candid_replay_audio", int(CFG.num("record", "audio_bitrate")), 0);
	if (!out || !v || !a) {
		if (error)
			*error = tr("The replay buffer could not start.");
		return false;
	}
	obs_output_set_video_encoder(out, v);
	obs_output_set_audio_encoder(out, a, 0);
	watch(out, "replay");
	signal_handler_connect(obs_output_get_signal_handler(out), "saved", onReplaySaved, this);
	m_replayOut = out.Get();
	if (!obs_output_start(out)) {
		if (error)
			*error = stopMessage(out, OBS_OUTPUT_ERROR);
		m_replayOut = nullptr;
		return false;
	}
	m_replay = true;
	emit stateChanged();
	return true;
}

void Outputs::stopReplay()
{
	if (m_replayOut)
		obs_output_stop(m_replayOut);
}

void Outputs::saveReplay()
{
	if (!m_replayOut)
		return;
	calldata_t cd = {};
	proc_handler_call(obs_output_get_proc_handler(m_replayOut), "save", &cd);
	calldata_free(&cd);
}

// Virtual camera --------------------------------------------------------------------------------------

bool Outputs::virtualCamAvailable() const
{
	return hasOutput("virtualcam_output");
}

bool Outputs::startVirtualCam(QString *error)
{
	if (m_vcam)
		return true;
	if (!virtualCamAvailable()) {
		if (error)
			*error = tr("The virtual camera isn't installed. Reinstalling CocoaCandid sets it up.");
		return false;
	}
	OBSOutputAutoRelease out = obs_output_create("virtualcam_output", "candid_virtualcam", nullptr, nullptr);
	if (!out)
		return false;
	obs_output_set_media(out, obs_get_video(), obs_get_audio());
	watch(out, "vcam");
	m_vcamOut = out.Get();
	if (!obs_output_start(out)) {
		if (error)
			*error = stopMessage(out, OBS_OUTPUT_ERROR);
		m_vcamOut = nullptr;
		return false;
	}
	m_vcam = true;
	emit stateChanged();
	return true;
}

void Outputs::stopVirtualCam()
{
	if (m_vcamOut)
		obs_output_stop(m_vcamOut);
}

void Outputs::stopAll()
{
	for (OBSOutput *o : {&m_record, &m_stream, &m_replayOut, &m_vcamOut})
		if (*o)
			obs_output_force_stop(*o);
	m_record = m_stream = m_replayOut = m_vcamOut = nullptr;
	m_service = nullptr;
	m_recording = m_streaming = m_replay = m_vcam = m_paused = m_streamStarting = false;
}

// Stats -----------------------------------------------------------------------------------------------

OutputStats Outputs::stats()
{
	OutputStats s;
	if (!m_cpu)
		m_cpu = os_cpu_usage_info_start();
	s.cpu = os_cpu_usage_info_query(static_cast<os_cpu_usage_info_t *>(m_cpu));
	s.fps = obs_get_active_fps();
	s.renderMs = double(obs_get_average_frame_time_ns()) / 1e6;
	s.laggedFrames = obs_get_lagged_frames();
	s.totalFrames = obs_get_total_frames();
	if (m_stream && m_streaming) {
		s.droppedStream = obs_output_get_frames_dropped(m_stream);
		s.totalStream = obs_output_get_total_frames(m_stream);
		const uint64_t bytes = obs_output_get_total_bytes(m_stream);
		const qint64 now = m_streamClock.elapsed();
		if (m_lastBytesAt && now > m_lastBytesAt && bytes >= m_lastBytes)
			m_kbps = double(bytes - m_lastBytes) * 8.0 / double(now - m_lastBytesAt);
		m_lastBytes = bytes;
		m_lastBytesAt = now;
		s.streamKbps = m_kbps;
	}
	if (m_record && m_recording)
		s.recordMb = double(obs_output_get_total_bytes(m_record)) / (1024.0 * 1024.0);
	return s;
}
