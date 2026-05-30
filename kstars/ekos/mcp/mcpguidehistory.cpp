/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mcpguidehistory.h"

#include "ekos/guide/guide.h"

#include <QDateTime>

namespace MCP
{

GuideHistory::GuideHistory(QObject *parent, int capacity)
    : QObject(parent), m_capacity(capacity > 0 ? capacity : 600)
{
}

void GuideHistory::attach(Ekos::Guide *guide)
{
    if (!guide) return;
    // attach() is called every time MCP::Server::setGuide() runs, which the
    // Manager triggers twice during profile startup (once at module init,
    // once at INDI-ready). Lambda slots can't be deduplicated via
    // Qt::UniqueConnection, so drop any prior connections we made to this
    // guide before re-establishing them; otherwise each newAxisDelta would
    // produce two ring appends with identical content.
    disconnect(guide, nullptr, this, nullptr);

    // Each guide step emits newAxisDelta with the latest RA/DEC error. Sigma
    // is updated by a separate signal — we latch the most recent values and
    // include them in the next pushed sample so each entry is self-contained.
    connect(guide, &Ekos::Guide::newAxisDelta, this, [this](double ra, double de)
    {
        GuideSample s;
        s.timestampMs = QDateTime::currentMSecsSinceEpoch();
        s.raDelta     = ra;
        s.decDelta    = de;
        s.raSigma     = m_raSigma;
        s.decSigma    = m_decSigma;
        m_ring.append(s);
        while (m_ring.size() > m_capacity)
            m_ring.removeFirst();
    });
    connect(guide, &Ekos::Guide::newAxisSigma, this, [this](double ra, double de)
    {
        m_raSigma  = ra;
        m_decSigma = de;
    });
}

QVector<GuideSample> GuideHistory::samplesSince(qint64 sinceMs) const
{
    QVector<GuideSample> out;
    out.reserve(m_ring.size());
    for (const auto &s : m_ring)
        if (s.timestampMs >= sinceMs)
            out.append(s);
    return out;
}

void GuideHistory::clear()
{
    m_ring.clear();
}

} // namespace MCP
