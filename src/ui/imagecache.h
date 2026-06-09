#pragma once
#include <QObject>
#include <QPixmap>
#include <QMap>
#include <QNetworkAccessManager>

namespace ProtonSage {

class ImageCache : public QObject {
    Q_OBJECT
public:
    static ImageCache& instance();

    // Get cached image, or request download and return placeholder
    QPixmap gameHeader(int appId, const QSize& size);
    bool isLoaded(int appId) const;

signals:
    void imageReady(int appId);

private:
    ImageCache();
    void download(int appId);
    QString cachePath(int appId) const;

    QNetworkAccessManager* m_nam;
    QMap<int, QPixmap> m_cache;
    QMap<int, bool> m_loading;
};

} // namespace ProtonSage
