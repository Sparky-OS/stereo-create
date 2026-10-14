// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Daniel Campos Ramos <Capitain_Jack@yahoo.com>
#include "anaglyph.h"
#include "convertvideo.h"
#include "photo.h"
#include "probe.h"

#include <QFile>
#include <QImageReader>
#include <QPainter>
#include <QTemporaryDir>
#include <QTest>

using namespace StereoCreate;

class EngineTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void anaglyphMatchesTheMatrices_data();
    void anaglyphMatchesTheMatrices();
    void transfer();
    void jps();
    void mpo();
    void sideBySidePng_data();
    void sideBySidePng();
    void sterPadding();
    void outputNeverOverwrites();
    void probePictures();
    void eyesGraph_data();
    void eyesGraph();

private:
    static StereoPair redBluePair(QSize size);
    static QColor centre(const QImage &image);
};

StereoPair EngineTest::redBluePair(QSize size)
{
    QImage left(size, QImage::Format_RGB888);
    left.fill(QColor(220, 30, 30));
    QImage right(size, QImage::Format_RGB888);
    right.fill(QColor(30, 30, 220));
    return {left, right};
}

QColor EngineTest::centre(const QImage &image)
{
    return image.pixelColor(image.width() / 2, image.height() / 2);
}

void EngineTest::anaglyphMatchesTheMatrices_data()
{
    QTest::addColumn<int>("pair");
    QTest::newRow("red/cyan") << int(AnaglyphPair::RedCyan);
    QTest::newRow("red/cyan CRT") << int(AnaglyphPair::RedCyanCrt);
    QTest::newRow("green/magenta") << int(AnaglyphPair::GreenMagenta);
    QTest::newRow("amber/blue") << int(AnaglyphPair::AmberBlue);
}

void EngineTest::anaglyphMatchesTheMatrices()
{
    // Every value of every channel in both eyes against the matrix in double precision, in linear light
    QFETCH(int, pair);
    const auto colours = AnaglyphPair(pair);
    QImage left(256, 3, QImage::Format_RGB888);
    QImage right(256, 3, QImage::Format_RGB888);
    for (int x = 0; x < 256; ++x) {
        for (int c = 0; c < 3; ++c) {
            const int a = c == 0 ? x : c == 1 ? 255 - x : (x * 7) % 256;
            const int b = c == 0 ? (x * 3) % 256 : c == 1 ? x : 255 - x;
            left.setPixelColor(x, c, QColor(a, b, (a + b) / 2));
            right.setPixelColor(x, c, QColor(b, (a * 5) % 256, a));
        }
    }
    const QImage result = anaglyph(left, right, colours);
    const DuboisMatrix &m = duboisMatrix(colours);
    int worst = 0;
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 256; ++x) {
            const QColor l = left.pixelColor(x, y);
            const QColor r = right.pixelColor(x, y);
            const double ll[3] = {srgbToLinear(l.red() / 255.0), srgbToLinear(l.green() / 255.0), srgbToLinear(l.blue() / 255.0)};
            const double rl[3] = {srgbToLinear(r.red() / 255.0), srgbToLinear(r.green() / 255.0), srgbToLinear(r.blue() / 255.0)};
            const QColor got = result.pixelColor(x, y);
            const int channels[3] = {got.red(), got.green(), got.blue()};
            for (int c = 0; c < 3; ++c) {
                double v = 0;
                for (int k = 0; k < 3; ++k) {
                    v += m.left[c][k] * ll[k] + m.right[c][k] * rl[k];
                }
                const int expected = qRound(linearToSrgb(qBound(0.0, v, 1.0)) * 255);
                worst = qMax(worst, qAbs(expected - channels[c]));
            }
        }
    }
    QVERIFY2(worst <= 1, qPrintable(QStringLiteral("worst channel error %1").arg(worst)));
}

void EngineTest::transfer()
{
    for (int i = 0; i < 256; ++i) {
        QCOMPARE(qRound(linearToSrgb(srgbToLinear(i / 255.0)) * 255), i);
    }
    QCOMPARE(srgbToLinear(0), 0.0);
    QVERIFY(qAbs(srgbToLinear(1) - 1) < 1e-12);
}

void EngineTest::jps()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("pair.jps"));
    QString error;
    QVERIFY2(writeJps(redBluePair(QSize(64, 48)), path, 95, &error), qPrintable(error));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray data = file.readAll();
    // the descriptor: stereo, side by side, right view first, full size
    QVERIFY(data.contains(jpsDescriptorSegment()));
    QVERIFY(data.contains(QByteArrayLiteral("_JPSJPS_\x00\x04\x00\x00\x02\x01")));
    // cross view: the right (blue) view is the left half of the JPEG
    const QImage packed(path, "jpeg");
    QVERIFY(centre(packed.copy(0, 0, 64, 48)).blue() > 180);
    QVERIFY(centre(packed.copy(64, 0, 64, 48)).red() > 180);
    // and the shared policy reads it back as the pair it was
    const Stereo::Picture picture = Stereo::Picture::load(path, {}, Stereo::OutputMode::Stereo);
    QVERIFY(picture.isStereo());
    QVERIFY(centre(picture.left).red() > 180);
    QVERIFY(centre(picture.right).blue() > 180);
}

void EngineTest::mpo()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("pair.mpo"));
    QString error;
    QVERIFY2(writeMpo(redBluePair(QSize(64, 48)), path, 95, &error), qPrintable(error));
    QImageReader reader(path);
    QCOMPARE(reader.imageCount(), 2);
    QCOMPARE(reader.text(QStringLiteral("Stereo3DLayout")), QStringLiteral("multiViewLeftFirst"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray data = file.readAll();
    // MP Format (CIPA DC-007): two MPF segments, the second picture's with its attribute IFD
    QCOMPARE(data.count(QByteArrayLiteral("MPF\0")), 2);
    const Stereo::Picture picture = Stereo::Picture::load(path, {}, Stereo::OutputMode::Stereo);
    QVERIFY(picture.isStereo());
    QVERIFY(centre(picture.left).red() > 180);
    QVERIFY(centre(picture.right).blue() > 180);
}

void EngineTest::sideBySidePng_data()
{
    QTest::addColumn<bool>("leftFirst");
    QTest::addColumn<int>("width");
    QTest::newRow("png, views on a multiple of 8") << true << 64;
    QTest::newRow("png, padded") << true << 61;
    QTest::newRow("pns, cross view, padded") << false << 61;
}

void EngineTest::sideBySidePng()
{
    QFETCH(bool, leftFirst);
    QFETCH(int, width);
    QTemporaryDir dir;
    const QString path = dir.filePath(leftFirst ? QStringLiteral("pair.png") : QStringLiteral("pair.pns"));
    QString error;
    QVERIFY2(writeSideBySidePng(redBluePair(QSize(width, 40)), path, leftFirst, &error), qPrintable(error));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray data = file.readAll();
    // sTER right after IHDR, mode 1 for left first, 0 for cross view
    QCOMPARE(data.mid(33, 12), pngChunk("sTER", QByteArray(1, leftFirst ? '\x01' : '\x00')).left(12));
    const QImage packed(path, "png");
    QCOMPARE(packed.width(), width * 2 + ::StereoCreate::sterPadding(width));
    const Stereo::Picture picture = Stereo::Picture::load(path, {}, Stereo::OutputMode::Stereo);
    QVERIFY(picture.isStereo());
    QCOMPARE(picture.left.width(), width);
    QCOMPARE(centre(picture.left), QColor(220, 30, 30));
    QCOMPARE(centre(picture.right), QColor(30, 30, 220));
}

void EngineTest::sterPadding()
{
    // PNG Extensions, sTER: the right subimage starts on a multiple of 8 columns
    QCOMPARE(::StereoCreate::sterPadding(598), 2);
    QCOMPARE(::StereoCreate::sterPadding(600), 0);
    QCOMPARE(::StereoCreate::sterPadding(599), 1);
    QCOMPARE(::StereoCreate::sterPadding(1), 7);
}

void EngineTest::outputNeverOverwrites()
{
    QTemporaryDir dir;
    const QString source = dir.filePath(QStringLiteral("photo.mpo"));
    QFile(source).open(QIODevice::WriteOnly);
    const QString first = outputPath(source, QString(), QStringLiteral("mpo"));
    QVERIFY(first != source);
    QVERIFY(!QFile::exists(first));
    QCOMPARE(outputPath(source, QStringLiteral("-anaglyph-red-cyan"), QStringLiteral("jpg")), dir.filePath(QStringLiteral("photo-anaglyph-red-cyan.jpg")));
}

void EngineTest::probePictures()
{
    QTemporaryDir dir;
    QString error;
    // a marked pair, an unmarked pair by its shape (2:1), a flat picture
    QVERIFY(writeMpo(redBluePair(QSize(64, 48)), dir.filePath(QStringLiteral("marked.mpo")), 95, &error));
    QImage shape(128, 64, QImage::Format_RGB888);
    shape.fill(Qt::gray);
    QVERIFY(shape.save(dir.filePath(QStringLiteral("shape.png"))));
    QImage flat(64, 48, QImage::Format_RGB888);
    flat.fill(Qt::green);
    QVERIFY(flat.save(dir.filePath(QStringLiteral("flat.png"))));
    const Source marked = probe(dir.filePath(QStringLiteral("marked.mpo")), true);
    QVERIFY(marked.stereo);
    QCOMPARE(marked.origin, Stereo::Picture::Source::FileMark);
    const Source byShape = probe(dir.filePath(QStringLiteral("shape.png")), true);
    QVERIFY(byShape.stereo);
    QCOMPARE(byShape.origin, Stereo::Picture::Source::Shape);
    QVERIFY(!probe(dir.filePath(QStringLiteral("flat.png")), true).stereo);
}

void EngineTest::eyesGraph_data()
{
    QTest::addColumn<int>("layout");
    QTest::addColumn<bool>("rightFirst");
    QTest::addColumn<bool>("full");
    QTest::addColumn<QSize>("frame");
    QTest::addColumn<QSize>("eye");
    QTest::addColumn<QString>("first");
    QTest::newRow("full side by side") << int(Stereo::Picture::Layout::SideBySide) << false << true << QSize(3840, 1080) << QSize(1920, 1080)
                                       << QStringLiteral("crop=iw/2:ih:0:0");
    QTest::newRow("half side by side") << int(Stereo::Picture::Layout::SideBySide) << false << false << QSize(1920, 1080) << QSize(1920, 1080)
                                       << QStringLiteral("crop=iw/2:ih:0:0");
    QTest::newRow("half top and bottom, right first") << int(Stereo::Picture::Layout::TopAndBottom) << true << false << QSize(1920, 1080) << QSize(1920, 1080)
                                                      << QStringLiteral("crop=iw:ih/2:0:ih/2");
}

void EngineTest::eyesGraph()
{
    QFETCH(int, layout);
    QFETCH(bool, rightFirst);
    QFETCH(bool, full);
    QFETCH(QSize, frame);
    QFETCH(QSize, eye);
    QFETCH(QString, first);
    Source source;
    source.layout = Stereo::Picture::Layout(layout);
    source.order = rightFirst ? Stereo::Picture::EyeOrder::RightFirst : Stereo::Picture::EyeOrder::LeftFirst;
    source.fullViews = full;
    source.frameSize = frame;
    source.displaySize = frame;
    QSize size;
    const QString graph = ::StereoCreate::eyesGraph(source, &size);
    QCOMPARE(size, eye);
    // the left view is cut from the first place of its eye order
    QVERIFY2(graph.contains(QStringLiteral("[a]") + first), qPrintable(graph));
    QVERIFY(graph.contains(QStringLiteral("sidedata=mode=delete:type=STEREO3D")));
}

QTEST_MAIN(EngineTest)
#include "enginetest.moc"
