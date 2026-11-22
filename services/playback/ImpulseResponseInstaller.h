#pragma once
#include <QList>
#include <QObject>
#include <QThread>
#include <QStringList>
namespace EchoMusic
{
/// One file of a downloaded effect package. Market effects can ship an impulse
/// response and a VPF parameter file as an atomic pair, so an install owns an
/// ordered list of resources and only reports success once every file exists.
struct EffectResourceRequest
{
    QStringList urls;
    QString destination;
};

/// A single bounded install. Network callbacks and atomic file writes share a
/// worker event loop; cancellation releases both before closed is emitted.
class ImpulseResponseInstaller final : public QObject
{
    Q_OBJECT
public:
    explicit ImpulseResponseInstaller(QObject *parent = nullptr);
    ~ImpulseResponseInstaller() override;
    void install(quint64 generation, const QStringList &urls, const QString &destination);
    void install(quint64 generation, const QList<EffectResourceRequest> &resources);
    void cancel();
    void shutdown();
signals:
    void progress(quint64 generation, double value);
    void finished(quint64 generation, bool success, const QString &path, const QString &error);
    void closed();

private:
    QThread m_thread;
    QObject *m_worker;
    bool m_closing = false;
};
} // namespace EchoMusic
