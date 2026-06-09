#include "imagecache.h"
#include <QNetworkReply>
#include <QDir>
#include <QStandardPaths>
#include <QPainter>

namespace ProtonSage {

static QPixmap placeholderPixmap(const QSize& size) {
    QPixmap pm(size);
    pm.fill(QColor("#2a2a2a"));
    QPainter p(&pm);
    p.setPen(QColor("#555"));
    p.setFont(QFont("sans-serif", 10));
    p.drawText(pm.rect(), Qt::AlignCenter, "No image");
    p.end();
    return pm;
}

static const QString kSteamCDN = "https://steamcdn-a.akamaihd.net/steam/apps/%1/header.jpg";

ImageCache& ImageCache::instance() {
    static ImageCache ic;
    return ic;
}

ImageCache::ImageCache() {
    m_nam = new QNetworkAccessManager(this);
    // Ensure cache directory exists
    QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(dir);
}

QString ImageCache::cachePath(int appId) const {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return dir + QString("/game_%1.jpg").arg(appId);
}

bool ImageCache::isLoaded(int appId) const {
    return m_cache.contains(appId);
}

QPixmap ImageCache::gameHeader(int appId, const QSize& size) {
    // Return cached version if available
    if (m_cache.contains(appId)) {
        QPixmap pm = m_cache[appId];
        if (pm.size() != size)
            return pm.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        return pm;
    }

    // Check disk cache
    QString path = cachePath(appId);
    if (QFile::exists(path)) {
        QPixmap pm(path);
        if (!pm.isNull()) {
            if (pm.size() != size)
                pm = pm.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            m_cache[appId] = pm;
            return pm;
        }
    }

    // Start download if not already loading
    if (!m_loading.value(appId, false)) {
        m_loading[appId] = true;
        download(appId);
    }

    // Return placeholder
    return placeholderPixmap(size);
}

void ImageCache::download(int appId) {
    QUrl url(QString(kSteamCDN).arg(appId));
    QNetworkReply* reply = m_nam->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, appId]() {
        reply->deleteLater();
        m_loading[appId] = false;
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QPixmap pm;
            if (pm.loadFromData(data)) {
                m_cache[appId] = pm;
                // Save to disk cache
                QFile f(cachePath(appId));
                if (f.open(QIODevice::WriteOnly)) {
                    pm.save(&f, "JPG");
                }
                emit imageReady(appId);
            }
        }
    });
}

} // namespace ProtonSage
