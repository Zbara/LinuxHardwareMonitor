#pragma once

#include <QHash>
#include <QLocale>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTranslator>
#include <QVector>

#include <memory>

// Translator backed by a flat JSON object {"source text": "translation"}.
// Lookups ignore the context, so the same file serves qsTr() in QML and tr() in C++.
// Missing entries fall through to the source text.
class JsonTranslator : public QTranslator {
    Q_OBJECT
public:
    explicit JsonTranslator(QObject *parent = nullptr) : QTranslator(parent) {}

    bool loadFile(const QString &path); // also accepts :/resource paths
    bool loadJson(const QByteArray &json);
    int size() const { return static_cast<int>(m_map.size()); }

    QString translate(const char *context, const char *sourceText, const char *disambiguation = nullptr,
                      int n = -1) const override;
    bool isEmpty() const override { return m_map.isEmpty(); }

private:
    QHash<QString, QString> m_map;
};

// Owns the installed translators and switches language at runtime. English is the
// source language and needs no translator.
class Localization : public QObject {
    Q_OBJECT
public:
    struct Language {
        QString code;
        QString nativeName;
    };

    // `directory` holds <code>.json files (":/i18n" for the embedded ones).
    explicit Localization(QString directory = QStringLiteral(":/i18n"), QObject *parent = nullptr);
    ~Localization() override;

    static QVector<Language> languages();

    // Picks the language to use: `requested` when it is supported, otherwise the system
    // language when supported, otherwise "en". An empty request means "follow the system".
    static QString resolve(const QString &requested, const QStringList &systemUiLanguages);

    // Installs translators for the resolved language. Returns the language actually in use.
    QString setLanguage(const QString &requested);
    QString currentLanguage() const { return m_current; }

private:
    void removeTranslators();

    QString m_directory;
    QString m_current = QStringLiteral("en");
    std::unique_ptr<JsonTranslator> m_app;
    std::unique_ptr<QTranslator> m_qt;
};
