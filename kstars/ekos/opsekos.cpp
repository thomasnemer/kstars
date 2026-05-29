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

    // Populate token field
    mcpTokenEdit->setText(Options::mCPToken());

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
}

void OpsEkos::updateMCPStatus(const QString &text)
{
    mcpStatusLabel->setText(text);
}
