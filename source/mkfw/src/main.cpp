#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QMap>
#include <QDateTime>
#include <QCryptographicHash>
#include <QDataStream>

void packageFile(QFile &, QDir &, QString, QString);
void unpackageFile(QString);
void showInfo(QString);

class Header {
public:
    QString modelName;
    QString version;
    QString checksum;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("mkfw");
    QCoreApplication::setApplicationVersion("1.00");

    QCommandLineParser parser;
    parser.parse(QCoreApplication::arguments());
    const QStringList args = parser.positionalArguments();
    const QString command = args.isEmpty() ? QString() : args.first();

    if (command == "unpack") {
        parser.setApplicationDescription("mkfw helper\n\n"
                                         "ex. mkfw unpack -s <file>");

        //parser.clearPositionalArguments();
        parser.addHelpOption();
        parser.addPositionalArgument("unpack", "unpack your firmware.", "unpack [unpackage_options]");

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
        //QStringList supportList = getSupportModels();

        parser.setApplicationDescription("mkfw helper\n\n"
                                         "ex. mkfw -m <model> -v [version] -s <file> -d <folder>\n"
                                         "ex. mkfw -m <model> -v [version] -s <file>\n"
                                         "(If destination is not selected, mkfw will use current directory for destination.)\n\n"
                                         "For unpack help:\n"
                                         "mkfw unpack --help");
        parser.addHelpOption();

        QCommandLineOption modelNameOption(QStringList() << "m" << "model-name",
                                           "Select a model name <model name>.",
                                           "model name");
        parser.addOption(modelNameOption);

        QCommandLineOption versionOption(QStringList() << "v" << "firmware-version",
                                           "Select a firmware version <fw version>.",
                                           "firmware version");
        parser.addOption(versionOption);

        QCommandLineOption sourceFileOption(QStringList() << "s" << "source-file",
                                           "Select a source file <source file>.",
                                           "source file");
        parser.addOption(sourceFileOption);

        QCommandLineOption destFolderOption(QStringList() << "d" << "dest-folder",
                                            "Select a destination folder <destination folder>.",
                                            "destination folder");
        parser.addOption(destFolderOption);


        QCommandLineOption infoOption(QStringList() << "i" << "info",
                                            "Show firmware info <source file>.",
                                            "source file");
        parser.addOption(infoOption);

        parser.process(app);

        if(     !parser.value(modelNameOption).isEmpty() ||
                !parser.value(versionOption).isEmpty() ||
                !parser.value(sourceFileOption).isEmpty()) {

            QString sourceFilePath = QDir::cleanPath(QFileInfo(parser.value(sourceFileOption)).absoluteFilePath());

            QDir destFolder(QFileInfo(sourceFilePath).absoluteDir());;
            if(!parser.value(destFolderOption).isEmpty()) {
                destFolder = QDir(parser.value(destFolderOption));
            }

            QFile srcFile(sourceFilePath);

            qDebug() << sourceFilePath << "\n" << destFolder.absolutePath();
            packageFile(srcFile,
                        destFolder,
                        parser.value(modelNameOption),
                        parser.value(versionOption));
        }
        else if(!parser.value(infoOption).isEmpty()) {
            QString sourceFilePath = QDir::cleanPath(QFileInfo(parser.value(infoOption)).absoluteFilePath());
            showInfo(sourceFilePath);
        }
        else
            parser.showHelp();
    }
}

void packageFile(QFile &srcFile,
                 QDir &destFolder,
                 QString modelName,
                 QString version) {

    if(!srcFile.exists()) {
        qDebug() << "Source file is invalid.";
        return;
    }

    if(!destFolder.exists()) {
        qDebug() << "Destination folder is invalid.";
        return;
    }

    if(modelName.size() > 76) {
        qDebug() << "The length limitation of model name is 76";
        qDebug() << "Length of model name(" << modelName << ") is too long.";
        return;
    }

    if(version.size() > 82) {
        qDebug() << "The length limitation of package name is 82";
        qDebug() << "Length of version(" << version << ") is too long.";
        return;
    }

    qDebug() << "\n"
             << "============================================\n"
             << "	 mkfw version: " << QCoreApplication::applicationVersion() << "\n"
             << "============================================\n"
             << "\n";


    int headerSize = 200;
    srcFile.open(QIODevice::ReadOnly);
    QDataStream in(&srcFile);    // read the data serialized from the file
    QByteArray str(headerSize, 0);
    str.replace(0x00, modelName.size(), modelName.toLocal8Bit());

    QDate currentDate = QDate::currentDate();
    QString versionInHeader = version + currentDate.toString(".MMdd.yyyy");
    str.replace(0x4C/*76*/, versionInHeader.size(), versionInHeader.toLocal8Bit());

    QString outFileName("DLINK_%1_%2(%3.%4)");

    QFile outFile(destFolder.absolutePath() + "/" +
                  outFileName
                  .arg(modelName)
                  .arg(version)
                  .arg(version.section('.', 0, 1))
                  .arg(currentDate.toString("MMdd.yyyy")));
    if(outFile.exists())
        outFile.remove();

    outFile.open(QIODevice::WriteOnly);
    if(!outFile.isWritable()) {
        qDebug() << "File: " << QFileInfo(outFile).absoluteFilePath() << " could not be written.";
        outFile.close();
        return;
    }

    outFile.seek(200);

    int bufSize = 4096;
    QCryptographicHash hash(QCryptographicHash::Md5);
    while(!in.atEnd()) {
        QByteArray buf = srcFile.read(bufSize);
        hash.addData(buf);
        outFile.write(buf);
    }

    outFile.reset();
    QByteArray checkSum(hash.result().toHex());
    str.replace(0xA8/*168*/, checkSum.size(), checkSum);

    outFile.write(str);

    srcFile.close();
    outFile.close();

    qDebug() << "\n"
             << "NAS type:              " << modelName << "\n"
             << "firmware version1:     " << version << "\n"
             << "firmware version2:     " << version.section('.', 0, 1) << "\n"
             << "firmware build date:   " << currentDate.toString("yyyy/MM/dd") << "\n"
             << "\n"
             << "firmware checksum:     " << checkSum << "\n"
             << "\n"
             << "Firmware " << QFileInfo(outFile).absoluteFilePath() << " is created";

}


/* check package file header and get their values. */
bool isValidFile(QString sourceFile, Header &header ) {
    QFile file(sourceFile);
    if(!file.exists()) {
        qDebug() << "File: " << sourceFile << " dose not exist.";
        return false;
    }

    file.open(QIODevice::ReadOnly);

    QByteArray modelName(32, 0);
    modelName = file.read(32);

    file.seek(0x4C);
    QByteArray version(32, 0);
    version = file.read(32);

    file.seek(0xA8);
    QByteArray headerChkSum(32, 0);
    headerChkSum = file.read(32);

    if(modelName.isEmpty() || version.isEmpty() || headerChkSum.isEmpty()) {
        qDebug() << "File: " << sourceFile << " is invalid";
        file.close();
        return false;
    }

    file.close();

    header.modelName = modelName;
    header.version = version;
    header.checksum = headerChkSum;
    return true;

}

void unpackageFile(QString sourceFile) {

    Header header;
    if(!isValidFile(sourceFile, header))
        return;

    QFile file(sourceFile);
    file.open(QIODevice::ReadOnly);
    file.seek(200);

    QString outFilePath("fw.bin");

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

    if(QString(checkSum) != header.checksum) {
        outFile.remove();
        qDebug() << "File: " << QFileInfo(sourceFile).absoluteFilePath() << " - checksum is error.";
    }
    else
        qDebug() << "file unpack in: " << QFileInfo(outFilePath).absoluteFilePath();

}

void showInfo(QString sourceFile) {
    Header header;
    if(!isValidFile(sourceFile, header))
        return;

    int distanceLast = header.version.lastIndexOf('.') - header.version.size();
    int idx = header.version.lastIndexOf('.', distanceLast - 1);
    QString sTime = header.version.mid(idx + 1, header.version.size() - idx);

    QDate date = QDate::fromString(sTime, "MMdd.yyyy");

    qDebug() << "\n"
             << "NAS type:              " << header.modelName << "\n"
             << "firmware version1:     " << header.version.section('.', 0, 2) << "\n"
             << "firmware version2:     " << header.version.section('.', 0, 1) << "\n"
             << "firmware build date:   " << date.toString("yyyy/MM/dd") << "\n"
             << "\n"
             << "firmware checksum:     " << header.checksum << "\n"
             << "\n";

}
