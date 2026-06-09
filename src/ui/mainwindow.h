#pragma once
#include <QMainWindow>
#include <QListWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include "storage/database.h"
#include "core/models.h"

namespace ProtonSage {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onGameSelected(int row);
    void onSearchChanged(const QString& text);
    void onCopyPreview();

private:
    void setupUI();
    void loadGames();
    void showRecommendation(int appId);
    void rebuildPreview();

    // Data
    QString m_dbPath;
    Database* m_db = nullptr;
    SystemProfile m_profile;
    QList<InstalledGameStatus> m_games;
    Recommendation m_currentRec;
    int m_currentAppId = 0;

    // Models and selection tracking
    struct GameItem {
        int appId;
        QString name;
        int reportCount;
        bool hasReports;
        QString existingLaunchOptions;
    };
    QList<GameItem> m_gameItems;

    // UI widgets
    QWidget* m_centralWidget = nullptr;
    QListWidget* m_gameList = nullptr;
    QLineEdit* m_searchInput = nullptr;
    QStackedWidget* m_rightStack = nullptr;

    // Empty state
    QWidget* m_emptyPage = nullptr;

    // Recommendation page
    QWidget* m_recPage = nullptr;
    QLabel* m_recTitle = nullptr;
    QLabel* m_gameImage = nullptr;
    QLabel* m_recSummary = nullptr;
    QWidget* m_suggestionsContainer = nullptr;
    QList<class SuggestionCheckbox*> m_suggestionWidgets;
    QLabel* m_previewLabel = nullptr;
    QPushButton* m_copyButton = nullptr;
    QLabel* m_safetyLabel = nullptr;
    QString m_existingLaunchOptions;
    QLabel* m_existingLabel = nullptr;
    QLabel* m_sysProfileLabel = nullptr;
};

class SuggestionCheckbox : public QWidget {
    Q_OBJECT
public:
    SuggestionCheckbox(const Suggestion& s, QWidget* parent = nullptr);
    const Suggestion& suggestion() const { return m_suggestion; }
    bool isChecked() const;
    void setChecked(bool checked);

signals:
    void toggled();

private:
    Suggestion m_suggestion;
    QPushButton* m_checkbox = nullptr;
};

} // namespace ProtonSage
