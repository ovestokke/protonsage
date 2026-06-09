#include "mainwindow.h"
#include "advisor/advisor.h"
#include "steam/library.h"
#include "steam/paths.h"
#include "system/profile.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QDebug>
#include <QTimer>
#include <QDir>

namespace ProtonSage {

// ── Human-readable label for launch options ──────────────────────────

static QString friendlyLabel(const QString& snippet) {
    QString lower = snippet.toLower().trimmed();
    
    // Strip %command% suffix for cleaner display
    QString s = lower;
    int cmd = s.indexOf("%command%");
    if (cmd >= 0) s = s.left(cmd).trimmed();
    if (s.isEmpty()) {
        // Only game args after %command%
        s = lower.mid(cmd + 10).trimmed();
        if (!s.isEmpty()) return "Add game arguments: " + s;
        return "Launch options";
    }
    
    // Map common env vars to labels
    QMap<QString,QString> labels;
    labels["proton_enable_wayland=1"] = "Wayland support";
    labels["proton_enable_wayland=0"] = "Disable Wayland";
    labels["proton_use_ntsync=1"] = "NTSync synchronization";
    labels["proton_enable_nvapi=1"] = "NVIDIA NVAPI / DLSS";
    labels["proton_enable_nvapi=0"] = "Disable NVAPI";
    labels["proton_enable_hdr=1"] = "HDR support";
    labels["proton_log=1"] = "Debug logging";
    labels["proton_fsr4_upgrade=1"] = "FSR4 upscaling upgrade";
    labels["proton_hide_nvidia_gpu=1"] = "Hide NVIDIA GPU";
    labels["dxvk_async=1"] = "DXVK async compilation";
    labels["dxvk_hdr=1"] = "DXVK HDR";
    labels["dxvk_frame_rate"] = "Frame rate limit";
    labels["enable_hdr_wsi=1"] = "HDR WSI";
    labels["mangohud=1"] = "MangoHUD overlay";
    labels["winedlloverrides"] = "Wine DLL overrides";
    labels["sdl_videodriver"] = "SDL video driver";
    labels["__gl_shader_disk_cache"] = "Shader cache";
    labels["_force-wayland"] = "Force Wayland";
    labels["gamemoderun"] = "GameMode optimizations";
    labels["mangohud"] = "MangoHUD performance overlay";
    labels["gamescope"] = "Gamescope compositor";
    labels["prime-run"] = "NVIDIA Prime offload";
    labels["obs-gamecapture"] = "OBS game capture";
    labels["game-performance"] = "CPU performance mode";
    labels["dlss-swapper"] = "DLSS version swapper";
    labels["nostartupmovies"] = "Skip intro videos";
    labels["nostartupmovies"] = "Skip startup movies";
    labels["nomoviestartup"] = "Skip intro movies";
    
    // Try to match the snippet
    for (auto it = labels.begin(); it != labels.end(); ++it) {
        if (s.contains(it.key())) return it.value();
    }
    
    // Generic env var: show simplified
    if (s.contains('=')) {
        QString name = s.section('=', 0, 0).toUpper();
        // Strip common prefixes
        for (const QString& p : {"PROTON_", "DXVK_", "VKD3D_", "RADV_", "MESA_", "__GL_"}) {
            if (name.startsWith(p)) name = name.mid(p.length());
        }
        return name.replace('_', ' ').toLower();
    }
    
    // Wrapper or game arg
    if (!s.isEmpty()) {
        s.replace('-', ' ');
        if (s.length() < 30) return s;
        return s.left(27) + "...";
    }
    
    return "Setting";
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
    m_checkbox->setChecked(true);
    m_checkbox->setStyleSheet(
        "QPushButton { background: #1e1e1e; color: #76B900; border: 1px solid #555; border-radius: 4px; font-weight: bold; }"
        "QPushButton:checked { background: #76B900; color: #1a1a1a; }");

    QString label = QString("<b>%1</b>  <span style='color:#999; font-size:10px;'>%2 · %3× · sim %4%</span>")
        .arg(friendlyLabel(s.snippet).toHtmlEscaped(), s.confidence, QString::number(s.occurrences),
             QString::number(static_cast<int>(s.systemSimilarity * 100)));
    if (!s.snippet.isEmpty() && s.snippet != friendlyLabel(s.snippet)) {
        label += QString("<br><span style='color:#666; font-size:10px; font-family:monospace;'>%1</span>")
            .arg(s.snippet.toHtmlEscaped());
    }
    auto* textLabel = new QLabel(label);
    textLabel->setWordWrap(true);
    textLabel->setStyleSheet("color: #e0e0e0; font-size: 12px;");

    layout->addWidget(m_checkbox);
    layout->addWidget(textLabel, 1);

    setStyleSheet("SuggestionCheckbox { background: #262626; border: 1px solid #3a3a3a; border-radius: 6px; }"
                  "SuggestionCheckbox:hover { border-color: #76B900; }");

    connect(m_checkbox, &QPushButton::toggled, this, &SuggestionCheckbox::toggled);
}

bool SuggestionCheckbox::isChecked() const { return m_checkbox->isChecked(); }

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

    // ── Left sidebar ────────────────────────────────────────────
    auto* leftPanel = new QWidget;
    leftPanel->setFixedWidth(300);
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
    m_gameList->setStyleSheet("QListWidget { background: #1e1e1e; }");
    leftLayout->addWidget(m_gameList, 1);
    connect(m_gameList, &QListWidget::currentRowChanged, this, &MainWindow::onGameSelected);

    // System profile
    auto* sysLabel = new QLabel("System: detecting...");
    sysLabel->setStyleSheet("color: #777; font-size: 10px; padding: 8px 0 0 0; line-height: 1.5;");
    sysLabel->setWordWrap(true);
    leftLayout->addWidget(sysLabel);
    m_sysProfileLabel = sysLabel;

    rootLayout->addWidget(leftPanel);

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
    recScroll->setWidget(m_recPage);

    auto* recLayout = new QVBoxLayout(m_recPage);
    recLayout->setContentsMargins(20, 20, 20, 20);

    m_recTitle = new QLabel;
    m_recTitle->setStyleSheet("font-size: 20px; font-weight: bold; color: #e0e0e0;");
    recLayout->addWidget(m_recTitle);

    m_recSummary = new QLabel;
    m_recSummary->setWordWrap(true);
    m_recSummary->setStyleSheet(
        "color: #d0d0d0; background: #262626; border: 1px solid #3a3a3a; "
        "border-radius: 8px; padding: 12px; font-size: 13px; line-height: 1.5;");
    recLayout->addWidget(m_recSummary);

    m_suggestionsContainer = new QWidget;
    auto* suggLayout = new QVBoxLayout(m_suggestionsContainer);
    suggLayout->setContentsMargins(0, 0, 0, 0);
    recLayout->addWidget(m_suggestionsContainer);

    recLayout->addStretch();

    // Preview bar
    auto* previewBar = new QWidget;
    previewBar->setStyleSheet("background: #262626; border-top: 1px solid #3a3a3a;");
    auto* previewLayout = new QHBoxLayout(previewBar);
    previewLayout->setContentsMargins(16, 12, 16, 12);

    m_previewLabel = new QLabel("%command%");
    m_previewLabel->setStyleSheet("color: #e0e0e0; font-family: monospace; font-size: 13px;");
    m_previewLabel->setWordWrap(true);
    previewLayout->addWidget(m_previewLabel, 1);

    m_copyButton = new QPushButton("Copy");
    m_copyButton->setStyleSheet(
        "QPushButton { background: #76B900; color: #1a1a1a; padding: 8px 18px; "
        "border-radius: 6px; font-weight: bold; border: none; }"
        "QPushButton:hover { background: #8fd400; }");
    previewLayout->addWidget(m_copyButton);
    connect(m_copyButton, &QPushButton::clicked, this, &MainWindow::onCopyPreview);

    m_safetyLabel = new QLabel("Copy / export only — no Steam writes");
    m_safetyLabel->setStyleSheet("color: #76B900; font-size: 11px; padding: 0 16px;");
    previewLayout->addWidget(m_safetyLabel);

    m_existingLabel = new QLabel;
    m_existingLabel->setStyleSheet("color: #777; font-size: 11px; padding: 4px 16px 12px;");
    m_existingLabel->setWordWrap(true);

    auto* rightLayout = new QVBoxLayout;
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);
    rightLayout->addWidget(recScroll, 1);
    rightLayout->addWidget(previewBar);
    rightLayout->addWidget(m_existingLabel);

    auto* rightWrapper = new QWidget;
    rightWrapper->setLayout(rightLayout);
    m_rightStack->addWidget(rightWrapper);

    rootLayout->addWidget(m_rightStack, 1);
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
        m_gameList->addItem(listItem);
    }
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

    auto reports = m_db->reportsByAppId(appId);
    if (reports.isEmpty()) {
        m_recTitle->setText("No reports");
        m_recSummary->setText("No ProtonDB data available for this game.");
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
    m_recTitle->setText(game.name);

    // Summary
    m_recSummary->setText(m_currentRec.summary);

    // Suggestions
    // Clear old widgets
    for (auto* sw : m_suggestionWidgets) {
        m_suggestionsContainer->layout()->removeWidget(sw);
        sw->deleteLater();
    }
    m_suggestionWidgets.clear();

    int shown = 0;
    for (const auto& s : m_currentRec.suggestions) {
        if (s.kind == LaunchOptionKind::Diagnostic || s.kind == LaunchOptionKind::Note) continue;
        if (shown++ >= 15) break;
        auto* cb = new SuggestionCheckbox(s, m_suggestionsContainer);
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
    auto result = buildLaunchPreview(selected, m_existingLaunchOptions);
    m_previewLabel->setText(result.preview);
}

void MainWindow::onCopyPreview() {
    rebuildPreview();
    QApplication::clipboard()->setText(m_previewLabel->text());
    m_copyButton->setText("✓ Copied!");
    QTimer::singleShot(2000, this, [this]() { m_copyButton->setText("Copy"); });
}

} // namespace ProtonSage
