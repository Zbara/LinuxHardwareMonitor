#include "Localization.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibraryInfo>
#include <QRegularExpression>

bool JsonTranslator::loadFile(const QString &path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) && loadJson(f.readAll());
}

bool JsonTranslator::loadJson(const QByteArray &json) {
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return false;
    m_map.clear();
    const QJsonObject obj = doc.object();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        const QString text = it.value().toString();
        if (!text.isEmpty()) // an empty value means "not translated yet"
            m_map.insert(it.key(), text);
    }
    return true;
}

QString JsonTranslator::translate(const char *, const char *sourceText, const char *, int) const {
    return m_map.value(QString::fromUtf8(sourceText));
}

Localization::Localization(QString directory, QObject *parent)
    : QObject(parent), m_directory(std::move(directory)) {}

Localization::~Localization() {
    removeTranslators();
}

QVector<Localization::Language> Localization::languages() {
    return {
        {QStringLiteral("en"), QStringLiteral("English")},
        {QStringLiteral("ru"), QStringLiteral("Русский")},
        {QStringLiteral("uk"), QStringLiteral("Українська")},
        {QStringLiteral("de"), QStringLiteral("Deutsch")},
        {QStringLiteral("es"), QStringLiteral("Español")},
    };
}

QString Localization::resolve(const QString &requested, const QStringList &systemUiLanguages) {
    QStringList supported;
    for (const Language &l : languages())
        supported << l.code;

    if (!requested.isEmpty())
        return supported.contains(requested) ? requested : QStringLiteral("en");

    for (const QString &ui : systemUiLanguages) {
        // "ru-RU", "ru_RU.UTF-8", "ru" -> "ru"
        const QString lang = ui.left(ui.indexOf(QRegularExpression(QStringLiteral("[-_.@]")))).toLower();
        if (supported.contains(lang))
            return lang;
    }
    return QStringLiteral("en");
}

void Localization::removeTranslators() {
    if (m_app)
        QCoreApplication::removeTranslator(m_app.get());
    if (m_qt)
        QCoreApplication::removeTranslator(m_qt.get());
    m_app.reset();
    m_qt.reset();
}

QString Localization::setLanguage(const QString &requested) {
    const QString code = resolve(requested, QLocale::system().uiLanguages());
    removeTranslators();
    m_current = code;
    if (code == QLatin1String("en"))
        return m_current;

    auto app = std::make_unique<JsonTranslator>();
    if (app->loadFile(m_directory + QLatin1Char('/') + code + QStringLiteral(".json"))) {
        QCoreApplication::installTranslator(app.get());
        m_app = std::move(app);
    }

    // Qt's own strings (standard dialog buttons, ...), when the translation is installed.
    auto qt = std::make_unique<QTranslator>();
    if (qt->load(QLatin1String("qtbase_") + code, QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        QCoreApplication::installTranslator(qt.get());
        m_qt = std::move(qt);
    }
    return m_current;
}
