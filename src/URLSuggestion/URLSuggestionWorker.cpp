#include "BrowserApplication.h"
#include "BookmarkManager.h"
#include "FaviconStore.h"
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
    m_differenceHash(0),
    m_searchTermHash(0)
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
    m_searchWords = m_searchTerm.split(QLatin1Char(' '), QString::SkipEmptyParts);
    m_searchTermHasScheme = (m_searchTerm.startsWith(QLatin1String("HTTP"))
            || m_searchTerm.startsWith(QLatin1String("FILE"))
            || m_searchTerm.startsWith(QLatin1String("VIPER")));
    hashSearchTerm();

    m_suggestionFuture = QtConcurrent::run(this, &URLSuggestionWorker::searchForHits);
    m_suggestionWatcher->setFuture(m_suggestionFuture);
}

void URLSuggestionWorker::searchForHits()
{
    m_working.store(true);
    m_suggestions.clear();

    // Set upper bound on number of suggestions for each type of item being checked
    const int maxSuggestedBookmarks = 15, maxSuggestedHistory = 15;
    int numSuggestedBookmarks = 0, numSuggestedHistory = 0;

    FaviconStore *faviconStore = sBrowserApplication->getFaviconStore();

    // Store urls being suggested in a set to avoid duplication when checking different data sources
    QSet<QString> hits;

    BookmarkManager *bookmarkMgr = sBrowserApplication->getBookmarkManager();
    for (auto it : *bookmarkMgr)
    {
        if (!m_working.load())
            return;

        if (it->getType() != BookmarkNode::Bookmark)
            continue;

        if (isEntryMatch(it->getName().toUpper(), it->getURL().toUpper(), it->getShortcut().toUpper()))
        {
            auto suggestion = URLSuggestion(it->getIcon(), it->getName(), it->getURL(), true);
            hits.insert(suggestion.URL);
            m_suggestions.push_back(suggestion);

            if (++numSuggestedBookmarks == maxSuggestedBookmarks)
                break;
        }
    }

    std::sort(m_suggestions.begin(), m_suggestions.end(), [](const URLSuggestion &a, const URLSuggestion &b) {
        return a.URL.size() < b.URL.size();
    });

    if (!m_working.load())
        return;

    std::vector<URLSuggestion> histSuggestions;
    HistoryManager *historyMgr = sBrowserApplication->getHistoryManager();
    for (auto it = historyMgr->getHistIterBegin(); it != historyMgr->getHistIterEnd(); ++it)
    {
        if (!m_working.load())
            return;

        const QString &url = it->URL.toString();
        if (hits.contains(url))
            continue;

        if (isEntryMatch(it->Title.toUpper(), url.toUpper()))
        {
            auto suggestion = URLSuggestion(faviconStore->getFavicon(it->URL), it->Title, url, false);
            histSuggestions.push_back(suggestion);

            if (++numSuggestedHistory == maxSuggestedHistory)
                break;
        }
    }

    std::sort(histSuggestions.begin(), histSuggestions.end(),
              [](const URLSuggestion &a, const URLSuggestion &b) {
        return a.URL.size() < b.URL.size();
    });

    m_suggestions.insert(m_suggestions.end(), histSuggestions.begin(), histSuggestions.end());

    m_working.store(false);
}

bool URLSuggestionWorker::isEntryMatch(const QString &title, const QString &url, const QString &shortcut)
{
    if (isStringMatch(title) || (!shortcut.isEmpty() && m_searchTerm.startsWith(shortcut)))
        return true;

    if (m_searchWords.size() > 1)
    {
        for (const QString &word : words)
        {
            if (title.contains(word, Qt::CaseSensitive))
                return true;
        }
    }

    int prefix = url.indexOf(QLatin1String("://"));
    if (!m_searchTermHasScheme && prefix >= 0)
    {
        QString urlMutable = url;
        urlMutable = urlMutable.mid(prefix + 3);
        return isStringMatch(urlMutable);
    }

    return isStringMatch(url);
}

void URLSuggestionWorker::hashSearchTerm()
{
    m_searchTermHash = 0;

    const int needleLength = m_searchTerm.size();
    const QChar *needlePtr = m_searchTerm.constData();

    const quint64 radixLength = 256ULL;
    const quint64 prime = 72057594037927931ULL;

    m_differenceHash = quPow(radixLength, static_cast<quint64>(needleLength - 1)) % prime;

    for (int index = 0; index < needleLength; ++index)
        m_searchTermHash = (radixLength * m_searchTermHash + (needlePtr + index)->toLatin1()) % prime;
}

bool URLSuggestionWorker::isStringMatch(const QString &haystack)
{
    static const quint64 radixLength = 256ULL;
    static const quint64 prime = 72057594037927931ULL;

    const int needleLength = m_searchTerm.size();
    const int haystackLength = haystack.size();

    if (needleLength > haystackLength)
        return false;
    if (needleLength == 0)
        return true;

    const QChar *needlePtr = m_searchTerm.constData();
    const QChar *haystackPtr = haystack.constData();

    int i, j;
    quint64 t = 0;
    quint64 h = 1;

    // The value of h would be "pow(d, M-1)%q"
    for (i = 0; i < needleLength - 1; ++i)
        h = (h * radixLength) % prime;

    // Calculate the hash value of first window of text
    for (i = 0; i < needleLength; ++i)
        t = (radixLength * t + ((haystackPtr + i)->toLatin1())) % prime;

    for (i = 0; i <= haystackLength - needleLength; ++i)
    {
        if (m_searchTermHash == t)
        {
            for (j = 0; j < needleLength; j++)
            {
                if ((haystackPtr + i + j)->toLatin1() != (needlePtr + j)->toLatin1())
                    break;
            }

            if (j == needleLength)
                return true;
        }

        if (i < haystackLength - needleLength)
        {
            t = (radixLength * (t - (h * (haystackPtr + i)->toLatin1()))
                 + (haystackPtr + i + needleLength)->toLatin1()) % prime;
        }
    }

    return false;
}

