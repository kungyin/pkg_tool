#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QFile>
#include <QDir>
//#include <QSettings>
#include <QProcess>
#include <QMap>
#include <QDateTime>
#include <QCryptographicHash>
#include <QDataStream>


const QString PACKAGE_TEMP_FILE = "pkg.tgz";

void packageFile(QDir, QDir, QMap<QString, QString> &, QString, int);
void unpackageFile(QString);
QMap<QString, QString> getRC(QString);
void showModels(QStringList &);
QStringList getSupportModels();
bool isModelValid(QString);

const char Models[][32] = {
    "DNS-320L-B",
    "DNS-327L-B"
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("mkapkg");
    QCoreApplication::setApplicationVersion("1.02");

    QCommandLineParser parser;

//    parser.setApplicationDescription("mkapkg helper\n"
//                                     "Please use:\n\n"
//                                     "mkapkg pack -h\n"
//                                     "mkapkg unpack -h\n\n"
//                                     "for help.");

    // Call parse() to find out the positional arguments.
    parser.parse(QCoreApplication::arguments());

    const QStringList args = parser.positionalArguments();
    const QString command = args.isEmpty() ? QString() : args.first();

    if (command == "unpack") {
        parser.setApplicationDescription("mkapkg helper\n\n"
                                         "ex. mkapkg unpack -s <file>");

        //parser.clearPositionalArguments();
        parser.addHelpOption();
        parser.addPositionalArgument("unpack", "unpack your APP.", "unpack [unpackage_options]");

        QCommandLineOption sourceFileOption(QStringList() << "s" << "source-file",
                                           "Select a source file <source file>.",
                                           "source file");
        parser.addOption(sourceFileOption);

        parser.process(app);

        if(!parser.value(sourceFileOption).isEmpty()) {
            unpackageFile(parser.value(sourceFileOption));
        }
        else
            qDebug() << "You must select a source file.";
    }
    else {
        QStringList supportList = getSupportModels();

        parser.setApplicationDescription("mkapkg er\n\n"
                                         //"ex. mkapkg -m <model> -s <folder> -d <folder>\n"
                                         "ex. mkapkg -m <model> -s <folder>\n"
                                         "ex. mkapkg -m <model>\n"
                                         "(If source is not selected, mkapkg will use current path.\n)");
        //parser.clearPositionalArguments();
        parser.addHelpOption();
        //parser.addPositionalArgument("pack", "pack your APP.", "pack [package_options]");

        QCommandLineOption modelNameOption(QStringList() << "m" << "model-name",
                                           "Select a model name <model name>.",
                                           "model name");
        parser.addOption(modelNameOption);

        QCommandLineOption modelListOption(QStringList() << "l" << "model-list",
                                           "Select to show support model list.");
        parser.addOption(modelListOption);

        QCommandLineOption sourceFolderOption(QStringList() << "s" << "source-folder",
                                           "Select a source folder <source folder>.",
                                           "source folder",
                                           QDir::currentPath());
        parser.addOption(sourceFolderOption);

//        QCommandLineOption destFolderOption(QStringList() << "d" << "dest-folder",
//                                            "Select a destination folder <destination folder>.",
//                                            "destination folder"/*,
//                                                                               dir.absolutePath()*/);
//        parser.addOption(destFolderOption);

        parser.process(app);

        if(parser.isSet(modelListOption)) {
            parser.clearPositionalArguments();
            showModels(supportList);
            return 0;
        }

        bool bDestValid = true;

        QString sourceFolder = QDir::cleanPath(QDir(parser.value(sourceFolderOption)).absolutePath());
        QString destFolder;
        //if(!parser.isSet(destFolderOption)) {
            QDir dirSrc(sourceFolder);
            if(dirSrc.cdUp())
                destFolder = dirSrc.absolutePath();
            else
                bDestValid = false;
        //}

        QString rcPath = sourceFolder + "/apkg.rc";
        QFileInfo sourceFileInfo(rcPath);

//        qDebug() << "sourceFolder: " << sourceFolder;
//        qDebug() << "destFolder: " << destFolder;

        if(!sourceFileInfo.exists()) {
            qDebug() << "Source folder is invalid.";
        }
        else if(!bDestValid) {
            qDebug() << "Can not use upper of source folder to destination folder.";
        }
        else {
            if (supportList.contains(parser.value(modelNameOption))) {
                int i3rdPatry = 0;
                if(args.contains("1"))
                    i3rdPatry = 1;

                QMap<QString, QString> map = getRC(rcPath);
                packageFile(QDir(sourceFolder), QDir(destFolder),
                            map, parser.value(modelNameOption), i3rdPatry);
            }
            else {
                qDebug() << "ERROR: model_name is not specify.\n";
                parser.clearPositionalArguments();
                showModels(supportList);
            }
        }
    }

}

void packageFile(QDir sourceFolder,
                 QDir destFolder,
                 QMap<QString, QString> &map,
                 QString modelName,
                 int i3rdParty) {

//    if(!sourceFolder.endsWith("/"))
//        sourceFolder += "/";
//    if(!destFolder.endsWith("/"))
//        destFolder += "/";

    if(!sourceFolder.exists()) {
        qDebug() << "Source folder is invalid.";
        return;
    }

    if(!destFolder.exists()) {
        qDebug() << "Destination folder is invalid.";
        return;
    }

    if(sourceFolder.absolutePath().compare(destFolder.absolutePath()) == 0) {
//        qDebug() << sourceFolder.absolutePath();
//        qDebug() << destFolder.absolutePath();
        qDebug() << "Source directory can not be equel to destination directory";
        return;
    }

    if(modelName.size() > 10) {
        qDebug() << "The length limitation of model name is 10";
        qDebug() << "Length of model name(" << modelName << ") is too long.";
        return;
    }

    if(map.value("Package").size() > 66) {
        qDebug() << "The length limitation of package name is 66";
        qDebug() << "Length of package name(" << map.value("Package") << ") is too long.";
        return;
    }

    if(map.value("Version").size() > 10) {
        qDebug() << "The length limitation of version is 66";
        qDebug() << "Length of version(" << map.value("Version") << ") is too long.";
        return;
    }

    QString filePath = destFolder.absolutePath() + "/" + PACKAGE_TEMP_FILE;

    qDebug();
    qDebug() << "============================================";
    qDebug() << "	 mkapkg version: " << QCoreApplication::applicationVersion();
    qDebug() << "============================================";
    qDebug();

    QProcess process;
    QStringList list = QStringList()
             << "zcvf"
             << filePath
             << "-C" << destFolder.absolutePath()
             << sourceFolder.dirName();
    //qDebug() << list;
    process.setProcessChannelMode(QProcess::ForwardedChannels);
    process.start("tar", list);
    process.waitForFinished();

    QFile tgzFile(filePath);
    if(!tgzFile.exists()) {
        qDebug() << "File: " << filePath << " dose not exist.";
        return;
    }

    int headerSize = 200;
    tgzFile.open(QIODevice::ReadOnly);
    QDataStream in(&tgzFile);    // read the data serialized from the file
    QByteArray str(headerSize, 0);
    str.replace(0x00, modelName.size(), modelName.toLocal8Bit());
    QByteArray packageName(map.value("Package").toLocal8Bit());
    str.replace(0x0A, packageName.size(), packageName);

    QByteArray version(map.value("Version").toLocal8Bit());
    str.replace(0x4C/*76*/, version.size(), version);

    //str.replace(112, 1, QByteArray::fromHex("02"));
    //str.replace(120, 1, QByteArray::fromHex("08"));
    //str.replace(124, 1, QByteArray::fromHex("0e"));

    QByteArray devValue(QByteArray::fromHex("00"));
    if(i3rdParty)
        devValue = QByteArray::fromHex("01");
    str.replace(0x80/*128*/, 1, devValue);

    QString outFileName("%1 %2 Package v%3_%4");
    QFile outFile(destFolder.absolutePath() + "/" +
                  outFileName
                  .arg(modelName)
                  .arg(map.value("Package"))
                  .arg(map.value("Version"))
                  .arg(QDateTime::currentDateTime().toString("MMddyyyy")));
    if(outFile.exists())
        outFile.remove();

    outFile.open(QIODevice::WriteOnly);
    if(!outFile.isWritable()) {
        qDebug() << "File: " << filePath << " could not be written.";
        outFile.close();
        return;
    }

    outFile.seek(200);

    int bufSize = 4096;
    QCryptographicHash hash(QCryptographicHash::Md5);
    while(!in.atEnd()) {
        QByteArray buf = tgzFile.read(bufSize);
        hash.addData(buf);
        outFile.write(buf);
    }

    outFile.reset();
    QByteArray checkSum(hash.result().toHex());
    str.replace(0xA8/*168*/, checkSum.size(), checkSum);

    outFile.write(str);

    tgzFile.close();
    outFile.close();
    tgzFile.remove();

    qDebug();
    qDebug() << "Model name:		" << modelName;
    qDebug() << "Package name:		" << map.value("Package");
    qDebug() << "Package version:	" << map.value("Version");
    qDebug() << "Packager:	        " << map.value("Packager");
    qDebug();
    qDebug() << "Package checksum:	" << checkSum;
    qDebug();
    qDebug() << "Add-ons \"" << QFileInfo(outFile).absoluteFilePath() << "\" is created";

}


void unpackageFile(QString sourceFile) {

    QFile file(sourceFile);
    if(!file.exists()) {
        qDebug() << "File: " << sourceFile << " dose not exist.";
        return;
    }

    file.open(QIODevice::ReadOnly);

    //get package name
    file.seek(0x0A);
    QByteArray packageName(32, 0);
    packageName = file.read(32);
    file.seek(0xA8);
    QByteArray headerChkSum(32, 0);
    headerChkSum = file.read(32);
    if(packageName.isEmpty()) {
        qDebug() << "File: " << sourceFile << " is invalid";
        return;
    }

    if(headerChkSum.isEmpty()) {
        qDebug() << "File: " << sourceFile << " is invalid";
        return;
    }

    file.seek(200);

    QString outFilePath("apkg.tgz");

    QFile outFile(outFilePath);
    outFile.open(QIODevice::WriteOnly);
    if(!outFile.isWritable()) {
        qDebug() << "File: " << outFilePath << " could not be written.";
        file.close();
        outFile.close();
        return;
    }

    QCryptographicHash hash(QCryptographicHash::Md5);
    int bufSize = 4096;
    while(!file.atEnd()) {
        QByteArray buf = file.read(bufSize);
        hash.addData(buf);
        outFile.write(buf);
    }

    QByteArray checkSum(hash.result().toHex());

    file.close();
    outFile.close();

    if(checkSum != headerChkSum) {
        outFile.remove();
        qDebug() << "File: " << QFileInfo(sourceFile).absoluteFilePath() << " - checksum is error.";
    }
    else
        qDebug() << "file unpack in: " << QFileInfo(outFilePath).absoluteFilePath();

}


QMap<QString, QString> getRC(QString filePath) {
    QMap<QString, QString> ret;

    QFile file(filePath);
    file.open(QIODevice::ReadOnly);

    while(!file.atEnd()) {
        QString line = QString(file.readLine());
        QStringList fields = line.split(":");
        if(fields.size() == 2)
            ret.insert(fields.at(0).trimmed(), fields.at(1).trimmed());
    }
    file.close();
    return ret;
}

void showModels(QStringList &list) {
    QString listFormat;

    for(QString e : list)
        listFormat += QString("             %1\n").arg(e);
    listFormat += "\n" ;

    qDebug() << "Usage: mkapkg -m [model_name]\n"
            << qPrintable("Supported model_name:\n\n" + listFormat);
}

QStringList getSupportModels() {
    QStringList ret;

    QFile file("mkapkg.conf");
    if(QFileInfo(file).exists()) {
        if (file.open(QFile::ReadOnly)) {
            char buf[1024];
            while(file.readLine(buf, sizeof(buf)) != -1) {
                QString line = QString(buf).trimmed();
                if(!line.isEmpty())
                    ret << QString(buf).trimmed();
            }
            file.close();
        }
    }
    else {
        int iModelsNumber = sizeof(Models)/sizeof(Models[0]);
        for(int i = 0; i < iModelsNumber; i++)
            ret << QString(Models[i]);
    }
    return ret;
}

/* not be used now */
bool isModelValid(QString model) {

    bool ret = false;
    int iModelsNumber = sizeof(Models)/sizeof(Models[0]);
    for(int i = 0; i < iModelsNumber; i++) {
        if(QString(Models[i]).compare(model) == 0)
            ret = true;
    }
    return ret;
}

