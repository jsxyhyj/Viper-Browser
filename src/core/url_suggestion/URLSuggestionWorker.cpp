#include "BookmarkManager.h"
#include "BookmarkNode.h"
#include "FastHash.h"
#include "FaviconManager.h"
#include "HistoryManager.h"
#include "URLSuggestionWorker.h"

#include <algorithm>
#include <QtConcurrent>
#include <QSet>
#include <QUrl>

URLSuggestionWorker::URLSuggestionWorker(QObject *parent) :
    QObject(parent),
    m_working(false),
    m_searchTerm(),
    m_searchWords(),
    m_searchTermHasScheme(false),
    m_suggestionFuture(),
    m_suggestionWatcher(nullptr),
    m_suggestions(),
    m_searchTermWideStr(),
    m_differenceHash(0),
    m_searchTermHash(0),
    m_bookmarkManager(nullptr),
    m_faviconManager(nullptr),
    m_historyManager(nullptr)
{
    m_suggestionWatcher = new QFutureWatcher<void>(this);
    connect(m_suggestionWatcher, &QFutureWatcher<void>::finished, [this](){
        emit finishedSearch(m_suggestions);
    });
}

void URLSuggestionWorker::findSuggestionsFor(const QString &text)
{
    if (m_working)
    {
        m_working.store(false);
        m_suggestionFuture.cancel();
        m_suggestionFuture.waitForFinished();
    }

    m_searchTerm = text.toUpper();

    // Remove any http or https prefix from the term, since we do not want to
    // do a string check on URLs and potentially remove an HTTPS match because
    // the user only entered HTTP 
    QRegularExpression httpExpr(QLatin1String("^HTTP(S?)://"));
    m_searchTerm.replace(httpExpr, QString());

    m_searchWords = m_searchTerm.split(QLatin1Char(' '), QString::SkipEmptyParts);
    m_searchTermHasScheme = (m_searchTerm.startsWith(QLatin1String("FILE"))
            || m_searchTerm.startsWith(QLatin1String("VIPER")));
    hashSearchTerm();

    m_suggestionFuture = QtConcurrent::run(this, &URLSuggestionWorker::searchForHits);
    m_suggestionWatcher->setFuture(m_suggestionFuture);
}

void URLSuggestionWorker::setServiceLocator(const ViperServiceLocator &serviceLocator)
{
    m_bookmarkManager = serviceLocator.getServiceAs<BookmarkManager>("BookmarkManager");
    m_faviconManager  = serviceLocator.getServiceAs<FaviconManager>("FaviconManager");
    m_historyManager  = serviceLocator.getServiceAs<HistoryManager>("HistoryManager");
}

void URLSuggestionWorker::searchForHits()
{
    m_working.store(true);
    m_suggestions.clear();

    if (!m_bookmarkManager || !m_faviconManager || !m_historyManager)
    {
        m_working.store(false);
        return;
    }

    // Set upper bound on number of suggestions for each type of item being checked
    const int maxSuggestedBookmarks = 15, maxSuggestedHistory = 50;
    int numSuggestedBookmarks = 0, numSuggestedHistory = 0;

    // Store urls being suggested in a set to avoid duplication when checking different data sources
    QSet<QString> hits;

    for (auto it : *m_bookmarkManager)
    {
        if (!m_working.load())
            return;

        if (it->getType() != BookmarkNode::Bookmark)
            continue;

        const QString url = it->getURL().toString();
        if (isEntryMatch(it->getName().toUpper(), url.toUpper(), it->getShortcut().toUpper()))
        {
            auto suggestion = URLSuggestion(it->getIcon(), it->getName(), url, true, m_historyManager->getTimesVisited(url));
            hits.insert(suggestion.URL);
            m_suggestions.push_back(suggestion);

            if (++numSuggestedBookmarks == maxSuggestedBookmarks)
                break;
        }
    }

    std::sort(m_suggestions.begin(), m_suggestions.end(), [](const URLSuggestion &a, const URLSuggestion &b) {
        return a.VisitCount > b.VisitCount;
    });

    if (!m_working.load())
        return;

    std::vector<URLSuggestion> histSuggestions;
    for (const auto it : *m_historyManager)
    {
        if (!m_working.load())
            return;

        const QString &url = it.getUrl().toString();
        if (hits.contains(url))
            continue;

        if (isEntryMatch(it.getTitle().toUpper(), url.toUpper()))
        {
            auto suggestion = URLSuggestion(m_faviconManager->getFavicon(it.getUrl()), it.getTitle(), url, false, it.getNumVisits());
            histSuggestions.push_back(suggestion);

            if (++numSuggestedHistory == maxSuggestedHistory)
                break;
        }
    }

    std::sort(histSuggestions.begin(), histSuggestions.end(),
              [](const URLSuggestion &a, const URLSuggestion &b) {
        return a.VisitCount > b.VisitCount;
    });

    if (histSuggestions.size() > 25)
        histSuggestions.erase(histSuggestions.begin() + 25, histSuggestions.end());

    m_suggestions.insert(m_suggestions.end(), histSuggestions.begin(), histSuggestions.end());

    m_working.store(false);
}

bool URLSuggestionWorker::isEntryMatch(const QString &title, const QString &url, const QString &shortcut)
{
    if (!shortcut.isEmpty() && m_searchTerm.startsWith(shortcut))
        return true;

    // Special case for small search terms
    if (m_searchTerm.size() < 5)
        return isMatchForSmallSearchTerm(title, url);

    if (isStringMatch(title))
        return true;

    if (m_searchWords.size() > 1)
    {
        for (const QString &word : m_searchWords)
        {
            if (title.contains(word, Qt::CaseSensitive))
                return true;
        }
    }

    const int prefix = url.indexOf(QLatin1String("://"));
    if (!m_searchTermHasScheme && prefix >= 0)
    {
        QString urlMutable = url;
        urlMutable = urlMutable.mid(prefix + 3);
        return isStringMatch(urlMutable);
    }

    return isStringMatch(url);
}

bool URLSuggestionWorker::isMatchForSmallSearchTerm(const QString &title, const QString &url)
{
    QStringList nameParts = title.split(QLatin1Char(' '), QString::SkipEmptyParts);
    if (nameParts.empty())
        nameParts.push_back(title);
    for (const QString &part : nameParts)
    {
        if (part.startsWith(m_searchTerm))
            return true;
    }

    const int prefix = url.indexOf(QLatin1String("://"));
    if (!m_searchTermHasScheme && prefix >= 0)
    {
        QString urlMutable = url;
        urlMutable = urlMutable.mid(prefix + 3);
        if (m_searchTerm.at(0) != QLatin1Char('W') && urlMutable.startsWith(QLatin1String("WWW.")))
            urlMutable = urlMutable.mid(4);
        return urlMutable.startsWith(m_searchTerm);
    }
    return url.startsWith(m_searchTerm);
}

void URLSuggestionWorker::hashSearchTerm()
{
    m_searchTermWideStr = m_searchTerm.toStdWString();
    m_differenceHash = FastHash::getDifferenceHash(static_cast<quint64>(m_searchTerm.size()));
    m_searchTermHash = FastHash::getNeedleHash(m_searchTermWideStr);
}

bool URLSuggestionWorker::isStringMatch(const QString &haystack)
{
    std::wstring haystackWideStr = haystack.toStdWString();
    return FastHash::isMatch(m_searchTermWideStr, haystackWideStr, m_searchTermHash, m_differenceHash);
}

