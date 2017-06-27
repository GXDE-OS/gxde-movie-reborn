#include "mpv_proxy.h"
#include "mpv_glwidget.h"
#include "compositing_manager.h"
#include "playlist_model.h"
#include <mpv/client.h>

#include <QtWidgets>

#include <xcb/xproto.h>
#include <xcb/xcb_aux.h>
#include <QX11Info>

namespace dmr {
using namespace mpv::qt;

enum AsyncReplyTag {
    SEEK,
};


static inline bool command_async(mpv_handle *ctx, const QVariant &args, uint64_t tag)
{
    node_builder node(args);
    int err = mpv_command_node_async(ctx, tag, node.node());
    return err == 0;
}

static void mpv_callback(void *d)
{
    MpvProxy *mpv = static_cast<MpvProxy*>(d);
    QMetaObject::invokeMethod(mpv, "has_mpv_events", Qt::QueuedConnection);
}

MpvProxy::MpvProxy(QWidget *parent)
    :QWidget(parent)
{
    if (!CompositingManager::get().composited()) {
        setWindowFlags(Qt::FramelessWindowHint);
        setAttribute(Qt::WA_NativeWindow);
        qDebug() << "proxy hook winId " << this->winId();
    }

    _handle = Handle::FromRawHandle(mpv_init());
    if (CompositingManager::get().composited()) {
        _gl_widget = new MpvGLWidget(this, _handle);
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(_gl_widget);
        setLayout(layout);
    }

    _playlist = new PlaylistModel(_handle);

    _burstScreenshotTimer = new QTimer(this);
    _burstScreenshotTimer->setSingleShot(true);
    connect(_burstScreenshotTimer, &QTimer::timeout, this, &MpvProxy::stepBurstScreenshot);
}

MpvProxy::~MpvProxy()
{
    disconnect(this, &MpvProxy::has_mpv_events, this, &MpvProxy::handle_mpv_events);
    mpv_terminate_destroy(_handle);
    if (CompositingManager::get().composited()) {
        delete _gl_widget;
    }
}

mpv_handle* MpvProxy::mpv_init()
{
    mpv_handle *h = mpv_create();

    bool composited = CompositingManager::get().composited();
    
    set_property(h, "terminal", "yes");
    set_property(h, "msg-level", "all=v");

    if (composited) {
        set_property(h, "vo", "opengl-cb");
        set_property(h, "hwdec-preload", "auto");
        set_property(h, "hwdec", "auto");

    } else {
        set_property(h, "vo", "opengl,xv");
        set_property(h, "hwdec", "auto");
        set_property(h, "wid", this->winId());
    }

    set_property(h, "input-cursor", "no");
    set_property(h, "cursor-autohide", "no");

    set_property(h, "screenshot-template", "deepin-movie-shot%n");
    set_property(h, "screenshot-directory", "/tmp");
    

    //only to get notification without data
    mpv_observe_property(h, 0, "time-pos", MPV_FORMAT_NONE);
    mpv_observe_property(h, 0, "pause", MPV_FORMAT_NONE);
    mpv_observe_property(h, 0, "mute", MPV_FORMAT_NONE);
    mpv_observe_property(h, 0, "volume", MPV_FORMAT_NONE);
    mpv_observe_property(h, 0, "playlist-pos", MPV_FORMAT_NONE);
    mpv_observe_property(h, 0, "playlist-count", MPV_FORMAT_NONE);
    mpv_observe_property(h, 0, "core-idle", MPV_FORMAT_NODE);

    mpv_request_log_messages(h, "info");
    mpv_set_wakeup_callback(h, mpv_callback, this);
    connect(this, &MpvProxy::has_mpv_events, this, &MpvProxy::handle_mpv_events,
            Qt::DirectConnection);
    if (mpv_initialize(h) < 0) {
        std::runtime_error("mpv init failed");
    }

    return h;
}

void MpvProxy::setState(MpvProxy::CoreState s)
{
    if (_state != s) {
        _state = s;
        emit stateChanged();
    }
}

void MpvProxy::handle_mpv_events()
{
    while (1) {
        mpv_event* ev = mpv_wait_event(_handle, 0.005);
        if (ev->event_id == MPV_EVENT_NONE) 
            break;

        switch (ev->event_id) {
            case MPV_EVENT_LOG_MESSAGE:
                processLogMessage((mpv_event_log_message*)ev->data);
                break;

            case MPV_EVENT_PROPERTY_CHANGE:
                processPropertyChange((mpv_event_property*)ev->data);
                break;

            case MPV_EVENT_COMMAND_REPLY:
                if (ev->error < 0) {
                    qDebug() << "command error";
                }

                if (ev->reply_userdata == AsyncReplyTag::SEEK) {
                    this->_pendingSeek = false;
                }
                break;

            case MPV_EVENT_PLAYBACK_RESTART:
                // caused by seek or just playing
                break;

            case MPV_EVENT_FILE_LOADED:
                qDebug() << mpv_event_name(ev->event_id);
                if (_gl_widget) {
                    _gl_widget->setPlaying(true);
                }
                setState(CoreState::Playing); //might paused immediately
                emit fileLoaded();
                break;

            case MPV_EVENT_END_FILE:
                qDebug() << mpv_event_name(ev->event_id);
                if (_gl_widget) {
                    _gl_widget->setPlaying(false);
                }
                setState(CoreState::Idle);
                break;

            case MPV_EVENT_IDLE:
                qDebug() << mpv_event_name(ev->event_id);
                setState(CoreState::Idle);
                break;

            default:
                qDebug() << mpv_event_name(ev->event_id);
                break;
        }
    }
}

void MpvProxy::processLogMessage(mpv_event_log_message* ev)
{
    qDebug() << QString("%1:%2: %3").arg(ev->prefix).arg(ev->level).arg(ev->text);
}

void MpvProxy::processPropertyChange(mpv_event_property* ev)
{
    //if (ev->data == NULL) return;

    QString name = QString::fromUtf8(ev->name);
    if (name == "time-pos") {
        emit ellapsedChanged();
    } else if (name == "volume") {
        emit volumeChanged();
    } else if (name == "mute") {
        emit muteChanged();
    } else if (name == "pause") {
        if (get_property(_handle, "pause").toBool()) {
            setState(CoreState::Paused);
        } else {
            if (state() != CoreState::Idle)
                setState(CoreState::Playing);
        }
    } else if (name == "core-idle") {
    } else if (name == "playlist-pos") {
        _playlist->_current = get_property(_handle, "playlist-pos").toInt();
        emit _playlist->currentChanged();
    } else if (name == "playlist-count") {
        emit _playlist->countChanged();
    }
}

void MpvProxy::volumeUp()
{
    QList<QVariant> args = { "add", "volume", 2 };
    qDebug () << args;
    command(_handle, args);
}

void MpvProxy::changeVolume(int val)
{
    val = qMin(qMax(val, 0), 100);
    set_property(_handle, "volume", val);
}

void MpvProxy::volumeDown()
{
    QList<QVariant> args = { "add", "volume", -2 };
    qDebug () << args;
    command(_handle, args);
}

int MpvProxy::volume() const
{
    return get_property(_handle, "volume").toInt();
}

bool MpvProxy::muted() const
{
    return get_property(_handle, "mute").toBool();
}

void MpvProxy::toggleMute()
{
    QList<QVariant> args = { "cycle", "mute" };
    qDebug () << args;
    command(_handle, args);
}

void MpvProxy::play()
{
    if (!_playlist->count()) return;

    if (state() == CoreState::Idle) {
        _playlist->changeCurrent(0);
    }
}

void MpvProxy::prev()
{
    if (!_playlist->count()) return;

    _playlist->playPrev();
}

void MpvProxy::next()
{
    if (!_playlist->count()) return;

    _playlist->playNext();
}

void MpvProxy::clearPlaylist()
{
    if (!_playlist->count()) return;

    _playlist->clear();
}

void MpvProxy::pauseResume()
{
    if (_state == CoreState::Idle)
        return;

    set_property(_handle, "pause", !paused());
}

void MpvProxy::stop()
{
    QList<QVariant> args = { "stop" };
    qDebug () << args;
    command(_handle, args);
}

bool MpvProxy::paused()
{
    return _state == CoreState::Paused;
}

QPixmap MpvProxy::takeScreenshot()
{
    return takeOneScreenshot();
}

void MpvProxy::burstScreenshot()
{
    if (_inBurstShotting) {
        qWarning() << "already in burst screenshotting mode";
        return;
    }

    if (state() == CoreState::Idle)
        return;

    if (!paused()) pauseResume();
    _inBurstShotting = true;
    _burstScreenshotTimer->start();
}

QPixmap MpvProxy::takeOneScreenshot()
{
    if (state() == CoreState::Idle) return QPixmap();

    QList<QVariant> args = {"screenshot-raw"};
    node_builder node(args);
    mpv_node res;
    int err = mpv_command_node(_handle, node.node(), &res);
    if (err < 0) {
        qWarning() << "screenshot raw failed";
        return QPixmap();
    }

    node_autofree f(&res);

    Q_ASSERT(res.format == MPV_FORMAT_NODE_MAP);

    int w,h,stride;

    mpv_node_list *list = res.u.list;
    uchar *data = NULL;

    for (int n = 0; n < list->num; n++) {
        auto key = QString::fromUtf8(list->keys[n]);
        if (key == "w") {
            w = list->values[n].u.int64;
        } else if (key == "h") {
            h = list->values[n].u.int64;
        } else if (key == "stride") {
            stride = list->values[n].u.int64;
        } else if (key == "format") {
            auto format = QString::fromUtf8(list->values[n].u.string);
            qDebug() << "format" << format;
        } else if (key == "data") {
            data = (uchar*)list->values[n].u.ba->data;
        }
    }

    if (data) {
        //alpha should be ignored
        auto img = QPixmap::fromImage(QImage(data, w, h, stride, QImage::Format_RGB32));
        return img;
    }

    qDebug() << "failed";
    return QPixmap();
}

void MpvProxy::stepBurstScreenshot()
{
    if (!_inBurstShotting) {
        return;
    }

    QPixmap img = takeOneScreenshot();
    if (img.isNull()) {
        stopBurstScreenshot();
        return;
    }

    emit notifyScreenshot(img);

    {
        QList<QVariant> args = {"frame-step"};
        command(_handle, args);
    }

    _burstScreenshotTimer->start();
}

void MpvProxy::stopBurstScreenshot()
{
    _inBurstShotting = false;
    _burstScreenshotTimer->stop();
}

void MpvProxy::seekForward(int secs)
{
    if (state() == CoreState::Idle) return;

    //if (_pendingSeek) return;
    QList<QVariant> args = { "seek", QVariant(secs), "relative+keyframes" };
    qDebug () << args;
    command_async(_handle, args, AsyncReplyTag::SEEK);
    _pendingSeek = true;
}

void MpvProxy::seekBackward(int secs)
{
    if (state() == CoreState::Idle) return;

    //if (_pendingSeek) return;
    if (secs > 0) secs = -secs;
    QList<QVariant> args = { "seek", QVariant(secs), "relative+keyframes" };
    qDebug () << args;
    command_async(_handle, args, AsyncReplyTag::SEEK);
    _pendingSeek = true;
}

void MpvProxy::addPlayFile(const QFileInfo& fi)
{
    _playlist->append(fi);
}

qint64 MpvProxy::duration() const
{
    return get_property(_handle, "duration").value<qint64>();
}


qint64 MpvProxy::ellapsed() const
{
    return get_property(_handle, "time-pos").value<qint64>();
}

void MpvProxy::changeProperty(const QString& name, const QVariant& v)
{
}

} // end of namespace dmr

#include "mpv_proxy.moc"