/*
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cstdlib>
#include <stdio.h>

#include "Add.h"

#include <QCommandLineParser>
#include <QTextStream>

#include "cli/Utils.h"
#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "core/PasswordGenerator.h"

Add::Add()
{
    this->name = QString("add");
    this->description = QObject::tr("Add a new entry to a database.");
}

Add::~Add()
{
}

int Add::execute(QStringList arguments)
{

    QTextStream inputTextStream(stdin, QIODevice::ReadOnly);
    QTextStream outputTextStream(stdout, QIODevice::WriteOnly);

    QCommandLineParser parser;
    parser.setApplicationDescription(this->description);
    parser.addPositionalArgument("database", QObject::tr("Path of the database."));

    QCommandLineOption keyFile(QStringList() << "k"
                                             << "key-file",
                               QObject::tr("Key file of the database."),
                               QObject::tr("path"));
    parser.addOption(keyFile);

    QCommandLineOption username(QStringList() << "u"
                                              << "username",
                                QObject::tr("Username for the entry."),
                                QObject::tr("username"));
    parser.addOption(username);

    QCommandLineOption url(QStringList() << "url", QObject::tr("URL for the entry."), QObject::tr("URL"));
    parser.addOption(url);

    QCommandLineOption prompt(QStringList() << "p"
                                            << "password-prompt",
                              QObject::tr("Prompt for the entry's password."));
    parser.addOption(prompt);

    QCommandLineOption generate(QStringList() << "g"
                                              << "generate",
                                QObject::tr("Generate a password for the entry."));
    parser.addOption(generate);

    QCommandLineOption length(QStringList() << "l"
                                            << "password-length",
                              QObject::tr("Length for the generated password."),
                              QObject::tr("length"));
    parser.addOption(length);

    parser.addPositionalArgument("entry", QObject::tr("Path of the entry to add."));
    parser.process(arguments);

    const QStringList args = parser.positionalArguments();
    if (args.size() != 2) {
        outputTextStream << parser.helpText().replace("keepassxc-cli", "keepassxc-cli add");
        return EXIT_FAILURE;
    }

    QString databasePath = args.at(0);
    QString entryPath = args.at(1);

    Database* db = Database::unlockFromStdin(databasePath, parser.value(keyFile));
    if (db == nullptr) {
        return EXIT_FAILURE;
    }

    // Validating the password length here, before we actually create
    // the entry.
    QString passwordLength = parser.value(length);
    if (!passwordLength.isEmpty() && !passwordLength.toInt()) {
        qCritical("Invalid value for password length %s.", qPrintable(passwordLength));
        return EXIT_FAILURE;
    }

    Entry* entry = db->rootGroup()->addEntryWithPath(entryPath);
    if (!entry) {
        qCritical("Could not create entry with path %s.", qPrintable(entryPath));
        return EXIT_FAILURE;
    }

    if (!parser.value("username").isEmpty()) {
        entry->setUsername(parser.value("username"));
    }

    if (!parser.value("url").isEmpty()) {
        entry->setUrl(parser.value("url"));
    }

    if (parser.isSet(prompt)) {
        outputTextStream << "Enter password for new entry: ";
        outputTextStream.flush();
        QString password = Utils::getPassword();
        entry->setPassword(password);
    } else if (parser.isSet(generate)) {
        PasswordGenerator passwordGenerator;

        if (passwordLength.isEmpty()) {
            passwordGenerator.setLength(PasswordGenerator::DefaultLength);
        } else {
            passwordGenerator.setLength(passwordLength.toInt());
        }

        passwordGenerator.setCharClasses(PasswordGenerator::LowerLetters | PasswordGenerator::UpperLetters |
                                         PasswordGenerator::Numbers);
        QString password = passwordGenerator.generatePassword();
        entry->setPassword(password);
    }

    QString errorMessage = db->saveToFile(databasePath);
    if (!errorMessage.isEmpty()) {
        qCritical("Writing the database failed %s.", qPrintable(errorMessage));
        return EXIT_FAILURE;
    }

    outputTextStream << "Successfully added entry " << entry->title() << "." << endl;
    return EXIT_SUCCESS;
}
