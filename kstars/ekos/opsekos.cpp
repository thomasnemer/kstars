/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opsekos.h"

#include "manager.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "mcp/mcpserver.h"

#include <KConfigDialog>
#include <KLocalizedString>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>

OpsEkos::OpsEkos() : QTabWidget(KStars::Instance())
{
    setupUi(this);

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists("settings");

    connect(kcfg_EkosTopIcons, &QRadioButton::toggled, this, [this]()
    {
        if (Options::ekosTopIcons() != kcfg_EkosTopIcons->isChecked())
            KSNotification::info(i18n("You must restart KStars for this change to take effect."));
    });


    connect(analyzeAlternativeDirectoryB, &QPushButton::clicked, [this] ()
    {
        auto dir = QFileDialog::getExistingDirectory(
                       this, i18n("Set an alternate base directory for Analyze's captured images"),
                       QDir::homePath(),
                       QFileDialog::ShowDirsOnly);
        if (!dir.isEmpty())
            kcfg_AnalyzeAlternativeDirectoryName->setText(dir);
    });
    connect(kcfg_AnalyzeAlternativeDirectoryName, &QLineEdit::editingFinished, [this] ()
    {
        auto text = kcfg_AnalyzeAlternativeDirectoryName->text();

        QFileInfo newdir(text);
        if (text.size() > 0 && newdir.exists() && newdir.isDir())
            kcfg_AnalyzeAlternativeDirectoryName->setText(text);
    });

    // MCP: keep port spinner enabled only when the server is enabled
    connect(kcfg_MCPEnabled, &QCheckBox::toggled, kcfg_MCPPort, &QSpinBox::setEnabled);
    kcfg_MCPPort->setEnabled(kcfg_MCPEnabled->isChecked());

    // Live enable/disable the MCP server when the checkbox is toggled in the dialog
    connect(kcfg_MCPEnabled, &QCheckBox::toggled, this, [this](bool enabled) {
        auto *mgr = Ekos::Manager::Instance();
        if (!mgr)
            return;
        if (enabled)
        {
            mgr->ensureMCPServer();
        }
        else if (mgr->mcpServer())
        {
            mgr->mcpServer()->stop();
            mgr->updateMCPStatusLabel();
        }
    });

    // Restart on the new port when the user applies settings changes
    if (m_ConfigDialog)
    {
        connect(m_ConfigDialog, &KConfigDialog::settingsChanged, this, [this]() {
            auto *mgr = Ekos::Manager::Instance();
            if (!mgr || !mgr->mcpServer())
                return;
            if (Options::mCPEnabled() && mgr->mcpServer()->isListening()
                    && mgr->mcpServer()->port() != static_cast<quint16>(Options::mCPPort()))
            {
                if (!mgr->mcpServer()->restart(Options::mCPPort()))
                    updateMCPStatus(i18n("Failed: port in use"));
                else
                    mgr->updateMCPStatusLabel();
            }
        });
    }

    // Populate token fields
    mcpTokenEdit->setText(Options::mCPToken());
    mcpROTokenEdit->setText(Options::mCPReadOnlyToken());

    // Copy token to clipboard
    connect(mcpCopyTokenButton, &QPushButton::clicked, this, [this]()
    {
        QApplication::clipboard()->setText(mcpTokenEdit->text());
    });

    // Regenerate token
    connect(mcpRegenTokenButton, &QPushButton::clicked, this, [this]()
    {
        auto *mgr = Ekos::Manager::Instance();
        if (mgr && mgr->mcpServer())
            mgr->mcpServer()->regenerateToken();
        mcpTokenEdit->setText(Options::mCPToken());
    });

    // Copy read-only token to clipboard
    connect(mcpCopyROTokenButton, &QPushButton::clicked, this, [this]()
    {
        QApplication::clipboard()->setText(mcpROTokenEdit->text());
    });

    // Regenerate read-only token (lazy: generate on first click)
    connect(mcpRegenROTokenButton, &QPushButton::clicked, this, [this]()
    {
        auto *mgr = Ekos::Manager::Instance();
        if (mgr && mgr->mcpServer())
            mgr->mcpServer()->regenerateReadOnlyToken();
        mcpROTokenEdit->setText(Options::mCPReadOnlyToken());
    });
}

void OpsEkos::updateMCPStatus(const QString &text)
{
    mcpStatusLabel->setText(text);
}
