/*
    SPDX-FileCopyrightText: 2026 Thomas Nemer <thomas.nemer@fortytwo.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QVector>
#include <cstdint>

namespace Ekos
{
class Guide;
}

namespace MCP
{

struct GuideSample
{
    qint64 timestampMs = 0;
    double raDelta     = 0.0;   // arcsec
    double decDelta    = 0.0;   // arcsec
    double raSigma     = 0.0;   // arcsec RMS
    double decSigma    = 0.0;   // arcsec RMS
};

// Rolling fixed-capacity buffer of recent guide samples. Connected to
// Ekos::Guide::newAxisDelta + newAxisSigma to record one sample per guide
// step. ~10 minutes of history at 1 Hz cadence is plenty for trend
// detection (oscillation, drift, settling) without unbounded growth.
//
// Owned by MCP::Server alongside the LogBridge and EventBridge. Attaches
// to a live Ekos::Guide when Server::setGuide() is called; the connection
// auto-cleans when the Guide is destroyed.
class GuideHistory : public QObject
{
        Q_OBJECT
    public:
        explicit GuideHistory(QObject *parent = nullptr, int capacity = 600);

        void attach(Ekos::Guide *guide);

        int capacity() const { return m_capacity; }
        QVector<GuideSample> samples() const { return m_ring; }
        QVector<GuideSample> samplesSince(qint64 sinceMs) const;
        void clear();

    private:
        void pushSample();

        int                  m_capacity;
        QVector<GuideSample> m_ring;

        // Per-sample working state — newAxisDelta drives the push; newAxisSigma
        // latches the current sigma to include in the next pushed sample.
        double m_raSigma  { 0.0 };
        double m_decSigma { 0.0 };
};

} // namespace MCP
