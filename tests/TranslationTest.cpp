// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include <KLocalizedString>
#include <QTest>

class TranslationTest final : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void loadsSpanishCatalog();
};

void TranslationTest::loadsSpanishCatalog()
{
    const QByteArray domain = QByteArrayLiteral("linuxgitshell");
    KLocalizedString::setLanguages({QStringLiteral("es")});
    KLocalizedString::addDomainLocaleDir(domain, QStringLiteral(TRANSLATION_LOCALE_DIR));
    KLocalizedString::setApplicationDomain(domain);

    QCOMPARE(i18n("Repository loaded"), QStringLiteral("Repositorio cargado"));
    QCOMPARE(i18nd("linuxgitshell", "Open with LinuxGitShell"),
             QStringLiteral("Abrir con LinuxGitShell"));
    QCOMPARE(i18nd("linuxgitshell", "Could not start LinuxGitShell."),
             QStringLiteral("No se pudo iniciar LinuxGitShell."));
    QCOMPARE(i18nd("linuxgitshell", "Show Status"), QStringLiteral("Mostrar estado"));
    QCOMPARE(i18nd("linuxgitshell", "Merge in progress"), QStringLiteral("Fusión en curso"));
    QCOMPARE(i18nd("linuxgitshell", "Rebase in progress"), QStringLiteral("Rebase en curso"));
    QCOMPARE(i18nd("linuxgitshell", "Cherry-pick in progress"),
             QStringLiteral("Cherry-pick en curso"));
    QCOMPARE(i18nd("linuxgitshell", "Revert in progress"), QStringLiteral("Reversión en curso"));
    QCOMPARE(i18nd("linuxgitshell", "Bisect in progress"), QStringLiteral("Bisección en curso"));
}

QTEST_GUILESS_MAIN(TranslationTest)

#include "TranslationTest.moc"
