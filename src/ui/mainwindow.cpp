#include "mainwindow.h"
#include "imagecache.h"
#include "advisor/advisor.h"
#include "steam/library.h"
#include "steam/paths.h"
#include "system/profile.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QSplitter>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QDebug>
#include <QTimer>
#include <QDir>

namespace ProtonSage {

// ── Suggestion display helpers ──────────────────────────────────────

static QString confidenceLabel(const QString& c) {
    if (c == "high") return "Strongly recommended";
    if (c == "medium") return "Recommended";
    return "Try if needed";
}

static QString simLabel(double sim) {
    if (sim >= 0.5) return "Similar hardware";
    if (sim >= 0.2) return "Somewhat similar";
    return "Different hardware";
}

// Smart pre-selection: only auto-check suggestions that make sense for this system
static bool shouldPreselect(const Suggestion& s, const SystemProfile& profile) {
    QString lower = s.snippet.toLower();
    QString cat = s.category.toLower();
    
    // Always pre-select "strongly recommended" with high similarity
    if (s.confidence == "high" && s.systemSimilarity >= 0.5) return true;
    
    // Wayland: only if user is on Wayland
    if (lower.contains("wayland") || lower.contains("sdl_videodriver")) {
        if (profile.sessionType.toLower() == "wayland")
            return lower.contains("enable_wayland") || lower.contains("sdl_videodriver=wayland");
        return false;
    }
    
    // NVIDIA-specific: only if NVIDIA GPU, and only if it makes sense
    bool isNvidia = profile.gpuVendor.toLower() == "nvidia";
    if (cat == "nvidia" || lower.contains("nvapi") || lower.contains("dlss")
        || lower.contains("prime-run")) {
        if (!isNvidia) return false;
        // Prime offload: only on dual-GPU laptops, not desktop
        if (lower.contains("prime-run") || lower.contains("prime")) return false;
        // NVAPI/DLSS: safe default on NVIDIA
        return s.confidence != "low";
    }
    
    // HDR: skip by default (most people don't have HDR)
    if (lower.contains("hdr") || cat == "display") return false;
    
    // Performance: pre-select if high confidence
    if (cat == "performance" && s.confidence != "low") return true;
    
    // Compatibility fixes: only pre-select if medium+ confidence
    if (cat == "compatibility" && s.confidence != "low") return true;
    
    return false;
}

// ── SuggestionCheckbox ───────────────────────────────────────────────

SuggestionCheckbox::SuggestionCheckbox(const Suggestion& s, QWidget* parent)
    : QWidget(parent), m_suggestion(s)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);

    m_checkbox = new QPushButton(s.kind == LaunchOptionKind::Note || s.kind == LaunchOptionKind::Diagnostic ? "•" : "✓");
    m_checkbox->setFixedSize(28, 28);
    m_checkbox->setCheckable(true);
    m_checkbox->setChecked(false);
    m_checkbox->setStyleSheet(
        "QPushButton { background: #1e1e1e; color: #76B900; border: 1px solid #555; border-radius: 4px; font-weight: bold; }"
        "QPushButton:checked { background: #76B900; color: #1a1a1a; }");

    QString labelStr = s.label.isEmpty() ? s.snippet : s.label;
    
    QString text = QString("<b>%1</b>").arg(labelStr.toHtmlEscaped());
    
    // Description
    if (!s.description.isEmpty()) {
        text += QString("<br><span style='color:#999; font-size:11px;'>%1</span>")
            .arg(s.description.toHtmlEscaped());
    }
    
    // Meta line: confidence, reports, hardware match, category badge
    QStringList metaItems;
    metaItems << confidenceLabel(s.confidence);
    metaItems << QString("%1 reports").arg(s.occurrences);
    metaItems << simLabel(s.systemSimilarity);
    if (!s.category.isEmpty()) {
        metaItems << QString("<span style='color:#76B900;'>%1</span>").arg(s.category);
    }
    text += QString("<br><span style='color:#888; font-size:10px;'>%1</span>")
        .arg(metaItems.join(" · "));
    
    // Raw snippet in tiny monospace — normalize case
    {
        QString snippet = s.snippet;
        if (snippet.contains('=')) snippet = snippet.toUpper();  // env vars uppercase
        else snippet = snippet.toLower();  // wrappers/commands lowercase
        if (snippet.size() < 60) {
            text += QString("<br><span style='color:#555; font-size:9px; font-family:monospace;'>%1</span>")
                .arg(snippet.left(60).toHtmlEscaped());
        }
    }
    auto* textLabel = new QLabel(text);
    textLabel->setTextFormat(Qt::RichText);
    textLabel->setWordWrap(true);
    textLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    textLabel->setMinimumWidth(0);
    textLabel->setStyleSheet("color: #e0e0e0; font-size: 12px;");

    layout->addWidget(m_checkbox);
    layout->addWidget(textLabel, 1);

    setStyleSheet("SuggestionCheckbox { background: #262626; border: 1px solid #3a3a3a; border-radius: 6px; }"
                  "SuggestionCheckbox:hover { border-color: #76B900; }");

    connect(m_checkbox, &QPushButton::toggled, this, &SuggestionCheckbox::toggled);
    
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setMinimumWidth(0);
}

bool SuggestionCheckbox::isChecked() const { return m_checkbox->isChecked(); }

void SuggestionCheckbox::setChecked(bool checked) { m_checkbox->setChecked(checked); }

// ── MainWindow ────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    // XDG data path
    QString dataDir = qEnvironmentVariable("XDG_DATA_HOME",
        QDir::homePath() + "/.local/share");
    QDir().mkpath(dataDir + "/protonsage");
    m_dbPath = dataDir + "/protonsage/protonsage.db";

    setupUI();

    setWindowTitle("ProtonSage");
    resize(1200, 800);
    setMinimumSize(900, 600);

    // Global stylesheet
    setStyleSheet(R"(
        QMainWindow { background: #1a1a1a; }
        QLabel { color: #e0e0e0; }
        QLineEdit {
            background: #1e1e1e; border: 1px solid #4a4a4a; border-radius: 6px;
            padding: 6px 10px; color: #e0e0e0; font-size: 13px;
        }
        QLineEdit:focus { border-color: #76B900; }
        QListWidget {
            background: #1a1a1a; border: none; outline: none; color: #e0e0e0;
        }
        QListWidget::item {
            padding: 8px 12px; border-bottom: 1px solid #2a2a2a;
        }
        QListWidget::item:selected {
            background: #1a3a0a; color: #fff;
        }
        QScrollBar:vertical {
            background: #1a1a1a; width: 8px; margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #555; border-radius: 4px; min-height: 30px;
        }
        QScrollBar::handle:vertical:hover { background: #6a6a6a; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollArea { border: none; background: transparent; }
    )");

    // Defer heavy work until event loop is running
    QTimer::singleShot(50, this, [this]() {
        auto optDb = Database::open(m_dbPath);
        if (optDb) {
            m_db = new Database(std::move(*optDb));
        }
        m_profile = detectProfile();
        // Update system profile label
        if (m_sysProfileLabel) {
            QStringList lines;
            if (!m_profile.gpuVendor.isEmpty())
                lines << QString("%1 %2").arg(m_profile.gpuVendor, m_profile.gpuModel).simplified();
            if (!m_profile.gpuDriver.isEmpty())
                lines << QString("Driver %1").arg(m_profile.gpuDriver);
            if (!m_profile.cpu.isEmpty())
                lines << m_profile.cpu;
            if (m_profile.ramGb > 0)
                lines << QString("RAM %1 GB").arg(m_profile.ramGb, 0, 'f', 1);
            if (!m_profile.distro.isEmpty() || !m_profile.kernel.isEmpty())
                lines << QString("%1 · Linux %2").arg(m_profile.distro, m_profile.kernel);
            if (!m_profile.desktop.isEmpty() || !m_profile.sessionType.isEmpty())
                lines << QString("%1 (%2)").arg(m_profile.desktop, m_profile.sessionType);
            m_sysProfileLabel->setText(lines.join("\n"));
        }
        loadGames();
    });
}

void MainWindow::setupUI() {
    auto* central = new QWidget;
    setCentralWidget(central);
    auto* rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(4);
    splitter->setStyleSheet("QSplitter::handle { background: #3a3a3a; }");

    // ── Left sidebar ────────────────────────────────────────────
    auto* leftPanel = new QWidget;
    leftPanel->setMinimumWidth(200);
    leftPanel->setStyleSheet("background: #1e1e1e;");
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 12, 12, 12);

    // App title
    auto* title = new QLabel("ProtonSage");
    title->setStyleSheet("font-size: 16px; font-weight: bold; color: #76B900; padding: 4px 0;");
    leftLayout->addWidget(title);

    auto* subtitle = new QLabel("Local Proton intelligence");
    subtitle->setStyleSheet("color: #999; font-size: 11px; padding-bottom: 8px;");
    leftLayout->addWidget(subtitle);

    // Search
    m_searchInput = new QLineEdit;
    m_searchInput->setPlaceholderText("Filter games...");
    leftLayout->addWidget(m_searchInput);
    connect(m_searchInput, &QLineEdit::textChanged, this, &MainWindow::onSearchChanged);

    // Game list
    m_gameList = new QListWidget;
    m_gameList->setIconSize(QSize(92, 43));
    m_gameList->setStyleSheet("QListWidget { background: #1e1e1e; }");
    leftLayout->addWidget(m_gameList, 1);
    connect(m_gameList, &QListWidget::currentRowChanged, this, &MainWindow::onGameSelected);

    // System profile
    auto* sysLabel = new QLabel("System: detecting...");
    sysLabel->setStyleSheet("color: #777; font-size: 10px; padding: 8px 0 0 0; line-height: 1.5;");
    sysLabel->setWordWrap(true);
    leftLayout->addWidget(sysLabel);
    m_sysProfileLabel = sysLabel;

    splitter->addWidget(leftPanel);

    // ── Right panel ─────────────────────────────────────────────
    m_rightStack = new QStackedWidget;
    m_rightStack->setStyleSheet("background: #1a1a1a;");

    // Empty page
    m_emptyPage = new QWidget;
    auto* emptyLayout = new QVBoxLayout(m_emptyPage);
    emptyLayout->setAlignment(Qt::AlignCenter);
    auto* emptyLabel = new QLabel("Select a game from the list");
    emptyLabel->setStyleSheet("color: #555; font-size: 16px;");
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyLabel);
    m_rightStack->addWidget(m_emptyPage);

    // Recommendation page
    m_recPage = new QWidget;
    auto* recScroll = new QScrollArea;
    recScroll->setWidgetResizable(true);
    recScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    recScroll->setWidget(m_recPage);

    auto* recLayout = new QVBoxLayout(m_recPage);
    recLayout->setContentsMargins(20, 20, 20, 20);

    // Game image + title row
    auto* headerRow = new QHBoxLayout;
    headerRow->setAlignment(Qt::AlignTop);
    m_gameImage = new QLabel;
    m_gameImage->setFixedSize(184, 86);
    m_gameImage->setStyleSheet("background: #262626; border-radius: 8px;");
    m_gameImage->setAlignment(Qt::AlignCenter);
    headerRow->addWidget(m_gameImage);
    
    auto* titleCol = new QVBoxLayout;
    // Title + rating
    auto* titleRow = new QHBoxLayout;
    m_recTitle = new QLabel;
    m_recTitle->setStyleSheet("font-size: 20px; font-weight: bold; color: #e0e0e0;");
    titleRow->addWidget(m_recTitle);
    m_ratingBadge = new QLabel;
    m_ratingBadge->setStyleSheet("font-size: 11px; font-weight: bold; padding: 3px 10px; border-radius: 10px;");
    titleRow->addWidget(m_ratingBadge);
    titleRow->addStretch();
    titleCol->addLayout(titleRow);
    
    m_protondbLink = new QLabel;
    m_protondbLink->setStyleSheet("color: #76B900; font-size: 11px;");
    m_protondbLink->setCursor(Qt::PointingHandCursor);
    titleCol->addWidget(m_protondbLink);
    
    titleCol->setAlignment(Qt::AlignTop);
    headerRow->addLayout(titleCol, 1);
    recLayout->addLayout(headerRow);

    m_recSummary = new QLabel;
    m_recSummary->setWordWrap(true);
    m_recSummary->setStyleSheet(
        "color: #d0d0d0; background: #262626; border: 1px solid #3a3a3a; "
        "border-radius: 8px; padding: 12px; font-size: 13px; line-height: 1.5;");
    recLayout->addWidget(m_recSummary);

    m_suggestionsContainer = new QWidget;
    m_suggestionsContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_suggestionsContainer->setMinimumWidth(0);
    auto* suggLayout = new QVBoxLayout(m_suggestionsContainer);
    suggLayout->setContentsMargins(0, 0, 0, 0);
    recLayout->addWidget(m_suggestionsContainer);

    recLayout->addStretch();

    recLayout->addStretch();

    // ── Preview panel (bottom, resizable) ────────────────────────
    auto* previewPanel = new QWidget;
    previewPanel->setStyleSheet("background: #1a1a1a;");
    auto* previewPanelLayout = new QVBoxLayout(previewPanel);
    previewPanelLayout->setContentsMargins(12, 10, 12, 10);
    previewPanelLayout->setSpacing(8);

    // Preview header
    auto* previewHeader = new QHBoxLayout;
    auto* previewTitle = new QLabel("Launch options");
    previewTitle->setStyleSheet("color: #999; font-size: 11px; font-weight: bold; text-transform: uppercase;");
    previewHeader->addWidget(previewTitle);
    previewHeader->addStretch();
    auto* safetyBadge = new QLabel("Copy only — no Steam writes");
    safetyBadge->setStyleSheet("color: #76B900; font-size: 10px;");
    previewHeader->addWidget(safetyBadge);
    previewPanelLayout->addLayout(previewHeader);

    // Preview text
    m_previewLabel = new QLabel("%command%");
    m_previewLabel->setWordWrap(true);
    m_previewLabel->setStyleSheet(
        "color: #e0e0e0; font-family: monospace; font-size: 13px;"
        "background: #1e1e1e; border: 1px solid #3a3a3a; border-radius: 8px;"
        "padding: 10px 14px;");
    m_previewLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    previewPanelLayout->addWidget(m_previewLabel, 1);

    // Button row
    auto* buttonRow = new QHBoxLayout;
    m_copyButton = new QPushButton("Copy to clipboard");
    m_copyButton->setStyleSheet(
        "QPushButton { background: #76B900; color: #1a1a1a; padding: 8px 18px; "
        "border-radius: 6px; font-weight: bold; border: none; }"
        "QPushButton:hover { background: #8fd400; }");
    buttonRow->addWidget(m_copyButton);
    connect(m_copyButton, &QPushButton::clicked, this, &MainWindow::onCopyPreview);
    
    buttonRow->addStretch();
    m_existingLabel = new QLabel;
    m_existingLabel->setStyleSheet("color: #777; font-size: 11px;");
    m_existingLabel->setWordWrap(true);
    buttonRow->addWidget(m_existingLabel, 1);
    previewPanelLayout->addLayout(buttonRow);

    // ── Vertical splitter between rec area and preview ──────────
    auto* vSplitter = new QSplitter(Qt::Vertical);
    vSplitter->setHandleWidth(4);
    vSplitter->setStyleSheet("QSplitter::handle { background: #3a3a3a; }");
    vSplitter->addWidget(recScroll);
    vSplitter->addWidget(previewPanel);
    vSplitter->setSizes({500, 150});

    auto* rightLayout = new QVBoxLayout;
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);
    rightLayout->addWidget(vSplitter);

    auto* rightWrapper = new QWidget;
    rightWrapper->setLayout(rightLayout);
    m_rightStack->addWidget(rightWrapper);

    splitter->addWidget(m_rightStack);
    
    // Set initial sizes: sidebar 280px, rest to right panel
    splitter->setSizes({280, 800});
    rootLayout->addWidget(splitter);
}

void MainWindow::loadGames() {
    m_gameList->clear();
    m_gameItems.clear();

    auto roots = existingRoots();
    for (const auto& root : roots) {
        auto games = scanRoot(root);
        for (const auto& g : games) {
            GameItem item;
            item.appId = g.appId;
            item.name = g.name;
            item.existingLaunchOptions = g.existingLaunchOptions;
            if (m_db) {
                item.reportCount = m_db->reportCountByAppId(g.appId);
                item.hasReports = item.reportCount > 0;
            }
            m_gameItems.append(item);
        }
    }


    if (m_gameItems.isEmpty()) {
        auto* placeholder = new QListWidgetItem("No Steam games found");
        placeholder->setForeground(QColor("#555"));
        m_gameList->addItem(placeholder);
        return;
    }

    // Sort by name, games with reports first
    std::sort(m_gameItems.begin(), m_gameItems.end(), [](const GameItem& a, const GameItem& b) {
        if (a.hasReports != b.hasReports) return a.hasReports > b.hasReports;
        return a.name.toLower() < b.name.toLower();
    });

    for (const auto& item : m_gameItems) {
        QString text = item.name;
        if (item.hasReports) text += QString("  [%1]").arg(item.reportCount);
        auto* listItem = new QListWidgetItem(text);
        listItem->setData(Qt::UserRole, item.appId);
        if (item.hasReports) {
            listItem->setForeground(QColor("#e0e0e0"));
        } else {
            listItem->setForeground(QColor("#666"));
        }
        // Game header image from Steam CDN
        QPixmap icon = ImageCache::instance().gameHeader(item.appId, QSize(92, 43));
        listItem->setIcon(QIcon(icon));
        m_gameList->addItem(listItem);
    }
    
    // Update icons when images load
    connect(&ImageCache::instance(), &ImageCache::imageReady, this, [this](int appId) {
        // Update game list icon
        for (int i = 0; i < m_gameList->count(); ++i) {
            if (m_gameList->item(i)->data(Qt::UserRole).toInt() == appId) {
                QPixmap pm = ImageCache::instance().gameHeader(appId, QSize(92, 43));
                m_gameList->item(i)->setIcon(QIcon(pm));
                break;
            }
        }
        // Update recommendation header image if this is the current game
        if (m_currentAppId == appId) {
            QPixmap pm = ImageCache::instance().gameHeader(appId, QSize(184, 86));
            m_gameImage->setPixmap(pm.scaled(184, 86, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    });
}

void MainWindow::onSearchChanged(const QString& text) {
    for (int i = 0; i < m_gameList->count(); ++i) {
        auto* item = m_gameList->item(i);
        item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
    }
}

void MainWindow::onGameSelected(int row) {
    if (row < 0 || row >= m_gameItems.size()) return;
    const auto& item = m_gameItems[row];
    m_currentAppId = item.appId;
    m_existingLaunchOptions = item.existingLaunchOptions;
    showRecommendation(item.appId);
}

void MainWindow::showRecommendation(int appId) {
    if (!m_db) {
        m_rightStack->setCurrentIndex(0);
        return;
    }

    // Show game image even before data loads
    {
        QPixmap hdr = ImageCache::instance().gameHeader(appId, QSize(184, 86));
        m_gameImage->setPixmap(hdr.scaled(184, 86, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    auto reports = m_db->reportsByAppId(appId);
    
    // Get game name from Steam scan data (m_gameItems)
    QString gameName;
    for (const auto& item : m_gameItems) {
        if (item.appId == appId) { gameName = item.name; break; }
    }
    if (gameName.isEmpty()) gameName = QString("App %1").arg(appId);
    
    if (reports.isEmpty()) {
        m_recTitle->setText(gameName);
        m_protondbLink->setText("No ProtonDB data imported");
        m_protondbLink->setStyleSheet("color: #666; font-size: 11px;");
        m_recSummary->setText("Import a ProtonDB snapshot to see compatibility reports and launch options.");
        // Clear old suggestions
        for (auto* sw : m_suggestionWidgets) {
            m_suggestionsContainer->layout()->removeWidget(sw);
            sw->deleteLater();
        }
        m_suggestionWidgets.clear();
        m_previewLabel->setText("%command%");
        m_existingLabel->clear();
        m_rightStack->setCurrentIndex(1);
        return;
    }

    QList<Report> reportList;
    for (const auto& rec : reports) reportList.append(rec.report);

    Game game;
    game.appId = appId;
    game.name = reports.first().report.title.isEmpty() ? QString("App %1").arg(appId) : reports.first().report.title;

    m_currentRec = generateRecommendation(game, reportList, m_profile, QDateTime::currentDateTimeUtc());

    // Title
    m_recTitle->setText(gameName);
    
    // Computed rating badge (last 90 days, verdict-based)
    auto rating = m_db->gameRating(appId);
    QString badgeText, badgeColor;
    int pct = rating.pctYes();
    if (rating.total < 5)      { badgeText = "Few reports"; badgeColor = "#555"; }
    else if (pct >= 95)        { badgeText = "Excellent";   badgeColor = "#4CAF50"; }
    else if (pct >= 85)        { badgeText = "Good";        badgeColor = "#8BC34A"; }
    else if (pct >= 70)        { badgeText = "Playable";     badgeColor = "#FFC107"; }
    else if (pct >= 50)        { badgeText = "Issues";       badgeColor = "#FF9800"; }
    else                       { badgeText = "Borked";       badgeColor = "#F44336"; }
    m_ratingBadge->setText(badgeText);
    m_ratingBadge->setStyleSheet(QString("font-size: 11px; font-weight: bold; padding: 3px 10px; border-radius: 10px; background: %1; color: #1a1a1a;").arg(badgeColor));
    m_ratingBadge->setToolTip(QString("%1 recent reports: %2% recommended")
        .arg(rating.total).arg(pct));
    m_protondbLink->setStyleSheet("color: #76B900; font-size: 11px;");
    m_protondbLink->setText(QString("<a href='https://www.protondb.com/app/%1' style='color:#76B900; text-decoration:none;'>Open on ProtonDB</a>").arg(appId));
    m_protondbLink->setOpenExternalLinks(true);
    
    // Also fix the game name in the recommendation (was using DB title)
    game.name = gameName;
    
    // Refresh game image (already set before data check above)
    {
        QPixmap hdr = ImageCache::instance().gameHeader(appId, QSize(184, 86));
        m_gameImage->setPixmap(hdr.scaled(184, 86, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    // Summary
    m_recSummary->setText(m_currentRec.summary);

    // Suggestions
    // Clear old widgets
    for (auto* sw : m_suggestionWidgets) {
        m_suggestionsContainer->layout()->removeWidget(sw);
        sw->deleteLater();
    }
    m_suggestionWidgets.clear();

    // ── 1. Existing Steam options first (pre-checked) ──────────
    if (!m_existingLaunchOptions.isEmpty()) {
        QString existing = m_existingLaunchOptions;
        int cmdIdx = existing.toLower().indexOf("%command%");
        QString prefix = cmdIdx >= 0 ? existing.left(cmdIdx).trimmed() : "";
        for (const QString& token : prefix.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts)) {
            if (token == "%command%") continue;
            Suggestion s;
            s.snippet = token;
            s.kind = "env_var";
            s.id = "existing-" + token;
            s.occurrences = 1;
            s.systemSimilarity = 1.0;
            s.confidence = "high";
            s.category = "Current";
            s.description = "Currently set in Steam";
            auto meta = suggestionMeta(token);
            s.label = meta.label.isEmpty() ? token : meta.label;
            // Title-case the label for display
            if (!s.label.isEmpty() && s.label[0].isLower()) {
                s.label[0] = s.label[0].toUpper();
            }
            s.description = meta.desc.isEmpty() ? s.description : meta.desc;
            auto* cb = new SuggestionCheckbox(s, m_suggestionsContainer);
            cb->setChecked(true);
            qobject_cast<QVBoxLayout*>(m_suggestionsContainer->layout())->addWidget(cb);
            m_suggestionWidgets.append(cb);
            connect(cb, &SuggestionCheckbox::toggled, this, [this]() { rebuildPreview(); });
        }
    }

    // ── 2. ProtonDB suggestions (not pre-selected) ────────────
    int shown = 0;
    for (const auto& s : m_currentRec.suggestions) {
        if (s.kind == LaunchOptionKind::Diagnostic || s.kind == LaunchOptionKind::Note) continue;
        if (s.label.isEmpty() && s.kind != "launch_option") continue;
        if (shown++ >= 15) break;
        auto* cb = new SuggestionCheckbox(s, m_suggestionsContainer);
        // Never auto-select ProtonDB suggestions — only Current gets pre-checked
        qobject_cast<QVBoxLayout*>(m_suggestionsContainer->layout())->addWidget(cb);
        m_suggestionWidgets.append(cb);
        connect(cb, &SuggestionCheckbox::toggled, this, [this]() { rebuildPreview(); });
    }

    rebuildPreview();

    // Existing launch options
    if (!m_existingLaunchOptions.isEmpty()) {
        m_existingLabel->setText("Current Steam: " + m_existingLaunchOptions);
    } else {
        m_existingLabel->clear();
    }

    m_rightStack->setCurrentIndex(1);
}

void MainWindow::rebuildPreview() {
    QList<Suggestion> selected;
    for (const auto* sw : m_suggestionWidgets) {
        if (sw->isChecked()) selected.append(sw->suggestion());
    }
    if (selected.isEmpty()) {
        m_previewLabel->setText("%command%");
        return;
    }
    auto result = buildLaunchPreview(selected, "");
    // Normalize case: env vars uppercase, wrappers lowercase
    QStringList normalizedTokens;
    for (const QString& token : result.preview.split(' ', Qt::SkipEmptyParts)) {
        if (token == "%command%") normalizedTokens << "%command%";
        else if (token.contains('=')) normalizedTokens << token.toUpper();
        else normalizedTokens << token.toLower();
    }
    m_previewLabel->setText(normalizedTokens.join(' '));
}

void MainWindow::onCopyPreview() {
    rebuildPreview();
    QApplication::clipboard()->setText(m_previewLabel->text());
    m_copyButton->setText("✓ Copied!");
    QTimer::singleShot(2000, this, [this]() { m_copyButton->setText("Copy"); });
}

} // namespace ProtonSage
