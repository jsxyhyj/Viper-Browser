#include "Settings.h"

#include "HistoryManager.h"

#include <QDir>
#include <QFileInfo>
#include <QWebEngineSettings>
#include <QtWebEngineCoreVersion>

const QString Settings::Version = QStringLiteral("1.0");

Settings::Settings() :
    QObject(nullptr),
    m_firstRun(false),
    m_settings(),
    m_storagePath(),
    m_settingMap {
        { BrowserSetting::StoragePath, QLatin1String("StoragePath") },                { BrowserSetting::BookmarkPath, QLatin1String("BookmarkPath") },
        { BrowserSetting::ExtensionStoragePath, QLatin1String("ExtStoragePath") },    { BrowserSetting::HistoryPath, QLatin1String("HistoryPath") },
        { BrowserSetting::FaviconPath, QLatin1String("FaviconPath") },                { BrowserSetting::SearchEnginesFile, QLatin1String("SearchEnginesFile") },
        { BrowserSetting::SessionFile, QLatin1String("SessionFile") },                { BrowserSetting::UserAgentsFile, QLatin1String("UserAgentsFile") },
        { BrowserSetting::UserScriptsConfig, QLatin1String("UserScriptsConfig") },    { BrowserSetting::UserScriptsDir, QLatin1String("UserScriptsDir") },
        { BrowserSetting::AdBlockPlusConfig, QLatin1String("AdBlockPlusConfig") },    { BrowserSetting::ExemptThirdPartyCookieFile, QLatin1String("ExemptThirdPartyCookieFile") },
        { BrowserSetting::AdBlockPlusDataDir, QLatin1String("AdBlockPlusDataDir") },  { BrowserSetting::FixedFontSize, QLatin1String("FixedFontSize") },
        { BrowserSetting::HomePage, QLatin1String("HomePage") },                      { BrowserSetting::StartupMode, QLatin1String("StartupMode") },
        { BrowserSetting::NewTabPage, QLatin1String("NewTabPage") },                  { BrowserSetting::DownloadDir, QLatin1String("DownloadDir") },
        { BrowserSetting::SendDoNotTrack, QLatin1String("SendDoNotTrack") },          { BrowserSetting::AskWhereToSaveDownloads, QLatin1String("AskWhereToSaveDownloads") },
        { BrowserSetting::EnableJavascript, QLatin1String("EnableJavascript") },      { BrowserSetting::EnableJavascriptPopups, QLatin1String("EnableJavascriptPopups") },
        { BrowserSetting::AutoLoadImages, QLatin1String("AutoLoadImages") },          { BrowserSetting::EnablePlugins, QLatin1String("EnablePlugins") },
        { BrowserSetting::EnableCookies, QLatin1String("EnableCookies") },            { BrowserSetting::EnableThirdPartyCookies, QLatin1String("EnableThirdPartyCookies") },
        { BrowserSetting::EnableXSSAudit, QLatin1String("EnableXSSAudit") },          { BrowserSetting::CookiesDeleteWithSession, QLatin1String("CookiesDeleteWithSession") },
        { BrowserSetting::EnableBookmarkBar, QLatin1String("EnableBookmarkBar") },    { BrowserSetting::CustomUserAgent, QLatin1String("CustomUserAgent") },
        { BrowserSetting::UserScriptsEnabled, QLatin1String("UserScriptsEnabled") },  { BrowserSetting::AdBlockPlusEnabled, QLatin1String("AdBlockPlusEnabled") },
        { BrowserSetting::InspectorPort, QLatin1String("InspectorPort") },            { BrowserSetting::HistoryStoragePolicy, QLatin1String("HistoryStoragePolicy") },
        { BrowserSetting::StandardFont, QLatin1String("StandardFont") },              { BrowserSetting::ScrollAnimatorEnabled, QLatin1String("ScrollAnimatorEnabled") },
        { BrowserSetting::SerifFont, QLatin1String("SerifFont") },                    { BrowserSetting::OpenAllTabsInBackground, QLatin1String("OpenAllTabsInBackground") },
        { BrowserSetting::SansSerifFont, QLatin1String("SansSerifFont") },            { BrowserSetting::CursiveFont, QLatin1String("CursiveFont") },
        { BrowserSetting::FantasyFont, QLatin1String("FantasyFont") },                { BrowserSetting::FixedFont, QLatin1String("FixedFont") },
        { BrowserSetting::StandardFontSize, QLatin1String("StandardFontSize") },      { BrowserSetting::EnableAutoFill, QLatin1String("EnableAutoFill") },
        { BrowserSetting::CachePath, QLatin1String("CachePath") },                    { BrowserSetting::ThumbnailPath, QLatin1String("ThumbnailPath") },
        { BrowserSetting::FavoritePagesFile, QLatin1String("FavoritePagesFile") },    { BrowserSetting::Version, QLatin1String("Version") }
    }
{
    setObjectName(QLatin1String("Settings"));

    // Check if defaults need to be set
    if (!m_settings.contains(QLatin1String("StoragePath")))
        setDefaults();

    // Check if settings needs to be updated
    if (m_settings.value(QLatin1String("Version")).toString().compare(Version) != 0)
        updateSettings();

    m_storagePath = m_settings.value(QLatin1String("StoragePath")).toString();
}

QString Settings::getPathValue(BrowserSetting key)
{
    return m_storagePath + m_settings.value(m_settingMap.value(key, QLatin1String("unknown"))).toString();
}

QVariant Settings::getValue(BrowserSetting key)
{
    return m_settings.value(m_settingMap.value(key, QLatin1String("unknown")));
}

void Settings::setValue(BrowserSetting key, const QVariant &value)
{
    if (getValue(key) == value)
        return;

    m_settings.setValue(m_settingMap.value(key, QLatin1String("unknown")), value);

    emit settingChanged(key, value);
}

bool Settings::firstRun() const
{
    return m_firstRun;
}

void Settings::setDefaults()
{
    m_firstRun = true;

    QFileInfo settingsFileInfo(m_settings.fileName());

#ifdef Q_OS_WIN
    QDir settingsDir = QDir(QDir::homePath() + QString("\\AppData\\Local\\Vaccarelli"));
#else
    QDir settingsDir = settingsFileInfo.absoluteDir();
#endif
    QString settingsPath = settingsDir.absolutePath();
    if (!settingsDir.exists())
        settingsDir.mkpath(settingsPath);
    settingsPath.append(QDir::separator());

    QString cachePath =
            QString("%1%2%3%2%4").arg(QDir::homePath()).arg(QDir::separator()).arg(QLatin1String(".cache")).arg(QLatin1String("Vaccarelli"));

    m_settings.setValue(QLatin1String("StoragePath"), settingsPath);
    m_settings.setValue(QLatin1String("CachePath"), cachePath);
    m_settings.setValue(QLatin1String("BookmarkPath"), QLatin1String("bookmarks.db"));
    m_settings.setValue(QLatin1String("CookiePath"), QLatin1String("cookies.db"));
    m_settings.setValue(QLatin1String("ExtStoragePath"), QLatin1String("extension_storage.db"));
    m_settings.setValue(QLatin1String("HistoryPath"), QLatin1String("history.db"));
    m_settings.setValue(QLatin1String("FaviconPath"), QLatin1String("favicons.db"));
    m_settings.setValue(QLatin1String("FavoritePagesFile"), QLatin1String("favorite_pages.json"));
    m_settings.setValue(QLatin1String("ThumbnailPath"), QLatin1String("web_thumbnails.db"));
    m_settings.setValue(QLatin1String("UserAgentsFile"),  QLatin1String("user_agents.json"));
    m_settings.setValue(QLatin1String("SearchEnginesFile"), QLatin1String("search_engines.json"));
    m_settings.setValue(QLatin1String("SessionFile"), QLatin1String("last_session.json"));
    m_settings.setValue(QLatin1String("UserScriptsDir"), QLatin1String("UserScripts"));
    m_settings.setValue(QLatin1String("UserScriptsConfig"), QLatin1String("user_scripts.json"));
    m_settings.setValue(QLatin1String("AdBlockPlusConfig"), QLatin1String("adblock_plus.json"));
    m_settings.setValue(QLatin1String("AdBlockPlusDataDir"), QLatin1String("AdBlockPlus"));
    m_settings.setValue(QLatin1String("ExemptThirdPartyCookieFile"), QLatin1String("exempt_third_party_cookies.txt"));

    m_settings.setValue(QLatin1String("HomePage"), QLatin1String("https://www.startpage.com/"));
    m_settings.setValue(QLatin1String("StartupMode"), static_cast<int>(StartupMode::LoadHomePage));
    m_settings.setValue(QLatin1String("NewTabPage"), static_cast<int>(NewTabType::HomePage));
    m_settings.setValue(QLatin1String("DownloadDir"), QDir::homePath() + QDir::separator() + "Downloads");
    m_settings.setValue(QLatin1String("AskWhereToSaveDownloads"), false);
    m_settings.setValue(QLatin1String("SendDoNotTrack"), false);
    m_settings.setValue(QLatin1String("EnableJavascript"), true);
    m_settings.setValue(QLatin1String("EnableJavascriptPopups"), false);
    m_settings.setValue(QLatin1String("AutoLoadImages"), true);
    m_settings.setValue(QLatin1String("EnablePlugins"), false);
    m_settings.setValue(QLatin1String("EnableCookies"), true);
    m_settings.setValue(QLatin1String("CookiesDeleteWithSession"), false);
    m_settings.setValue(QLatin1String("EnableThirdPartyCookies"), true);
    m_settings.setValue(QLatin1String("EnableXSSAudit"), true);
    m_settings.setValue(QLatin1String("EnableBookmarkBar"), false);
    m_settings.setValue(QLatin1String("CustomUserAgent"), false);
    m_settings.setValue(QLatin1String("UserScriptsEnabled"), true);
    m_settings.setValue(QLatin1String("AdBlockPlusEnabled"), true);
#if (QTWEBENGINECORE_VERSION < QT_VERSION_CHECK(5, 11, 0))
    m_settings.setValue(QLatin1String("InspectorPort"), 9477);
#endif
    m_settings.setValue(QLatin1String("HistoryStoragePolicy"), static_cast<int>(HistoryStoragePolicy::Remember));
    m_settings.setValue(QLatin1String("ScrollAnimatorEnabled"), false);
    m_settings.setValue(QLatin1String("OpenAllTabsInBackground"), false);

    QWebEngineSettings *webSettings = QWebEngineSettings::defaultSettings();
    m_settings.setValue(QLatin1String("StandardFont"), webSettings->fontFamily(QWebEngineSettings::StandardFont));
    m_settings.setValue(QLatin1String("SerifFont"), webSettings->fontFamily(QWebEngineSettings::SerifFont));
    m_settings.setValue(QLatin1String("SansSerifFont"), webSettings->fontFamily(QWebEngineSettings::SansSerifFont));
    m_settings.setValue(QLatin1String("CursiveFont"), webSettings->fontFamily(QWebEngineSettings::CursiveFont));
    m_settings.setValue(QLatin1String("FantasyFont"), webSettings->fontFamily(QWebEngineSettings::FantasyFont));
    m_settings.setValue(QLatin1String("FixedFont"), webSettings->fontFamily(QWebEngineSettings::FixedFont));

    m_settings.setValue(QLatin1String("StandardFontSize"), webSettings->fontSize(QWebEngineSettings::DefaultFontSize));
    m_settings.setValue(QLatin1String("FixedFontSize"), webSettings->fontSize(QWebEngineSettings::DefaultFixedFontSize));

    m_settings.setValue(QLatin1String("Version"), Version);
}

void Settings::updateSettings()
{
    const QString userVersion = m_settings.value(QLatin1String("Version")).toString();
    if (userVersion.isEmpty())
    {
        // Cache path is only new setting added with the first versioning of settings
        QString cachePath =
                QString("%1%2%3%2%4").arg(QDir::homePath()).arg(QDir::separator()).arg(QLatin1String(".cache")).arg(QLatin1String("Vaccarelli"));
        m_settings.setValue(QLatin1String("CachePath"), cachePath);
    }

    bool ok = false;
    float versionNumber = m_settings.value(QLatin1String("Version")).toFloat(&ok);
    if (!ok || versionNumber < 0.9f)
        m_settings.setValue(QLatin1String("ThumbnailPath"), QLatin1String("web_thumbnails.db"));
    if (!ok || versionNumber < 1.0f)
    {
        m_settings.setValue(QLatin1String("NewTabPage"), static_cast<int>(NewTabType::BlankPage));
        m_settings.setValue(QLatin1String("FavoritePagesFile"), QLatin1String("favorite_pages.json"));
    }

    m_settings.setValue(QLatin1String("Version"), Version);
}
