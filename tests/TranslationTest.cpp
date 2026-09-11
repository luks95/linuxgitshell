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
}

QTEST_GUILESS_MAIN(TranslationTest)

#include "TranslationTest.moc"
