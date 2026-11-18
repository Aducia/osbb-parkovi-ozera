#include "style.h"

#include <QApplication>
#include <QFont>
#include <QLabel>
#include <QPixmap>
#include <QSize>
#include <QPalette>
#include <QPushButton>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>

namespace {

// Кольори інтерфейсу (брендові):
//   зелений #69CC69 — бічна панель, акценти;  фон сторінок #E4EFF7;
//   сірий #5D5D61 — другорядний текст (основний текст — його темніший відтінок #2E2E32);
//   блакитний #86A9DF — кнопки, виділення. Картки й таблиці — світліший відтінок фону #F6FAFD.
const char *kStyleSheet = R"QSS(
QMainWindow, QDialog {
    background: #E4EFF7;
}
QWidget {
    color: #2E2E32;
}
QToolTip {
    background: #2E2E32;
    color: #FFFFFF;
    border: none;
    padding: 8x 8px;
}

/* ---------- Бічна панель ---------- */
QFrame#sidebar {
    background: #69CC69;
}
QLabel#appSubtitle {
    color: #1F4D1F;
    font-size: 10 pt;
    font-weight: 600;
    background: transparent;
}
QScrollArea#navScroll {
    background: transparent;
    border: none;
}
QWidget#navContainer {
    background: transparent;
}
QLabel#navSection {
    color: #1F4D1F;
    font-size: 10 pt;
    font-weight: 700;
    padding: 12px 14px 4px 14px;
    background: transparent;
}
QPushButton#navButton {
    text-align: left;
    padding: 8px 16px;
    border: none;
    border-radius: 8px;
    background: transparent;
    color: #2E2E32;
    font-size: 12pt;
}
QPushButton#navButton:hover {
    background: rgba(255, 255, 255, 0.35);
}
QPushButton#navButton:checked {
    background: #E4EFF7;
    color: #2E2E32;
    font-weight: 700;
}
QFrame#userBox {
    background: rgba(255, 255, 255, 0.30);
    border-radius: 10px;
}
QLabel#userName {
    color: #1E1E22;
    font-weight: 700;
    background: transparent;
}
QLabel#userRole {
    color: #1F4D1F;
    font-size: 10pt;
    background: transparent;
}
QPushButton#sideAction {
    background: transparent;
    border: 2px solid rgba(46, 46, 50, 0.45);
    border-radius: 8px;
    color: #2E2E32;
    padding: 8px 12px;
}
QPushButton#sideAction:hover {
    background: rgba(255, 255, 255, 0.45);
}

/* ---------- Заголовок сторінки ---------- */
QFrame#pageHeader {
    background: #F6FAFD;
    border-bottom: 2px solid #C6D6E4;
}
QLabel#pageTitle {
    font-size: 16pt;
    font-weight: 600;
    color: #2E2E32;
}
QLabel#pageSubtitle {
    color: #5D5D61;
}

/* ---------- Картки й панелі ---------- */
QFrame#card, QFrame#panel {
    background: #F6FAFD;
    border: 2px solid #C6D6E4;
    border-radius: 12px;
}
QLabel#cardTitle {
    color: #5D5D61;
    font-size: 16pt;
    font-weight: 400;
    background: transparent;
}
QLabel#cardValue {
    color: #2E2E32;
    font-size: 16pt;
    font-weight: 600;
    background: transparent;
}
QLabel#cardHint {
    color: #5D5D61;
    font-size: 14pt;
    font-weight: 500;
    background: transparent;
}
QLabel#panelTitle {
    font-size: 12pt;
    font-weight: 600;
    color: #2E2E32;
    background: transparent;
}
QLabel#hint {
    color: #5D5D61;
    background: transparent;
}
QLabel#summary {
    background: #D5E3F5;
    border: 1px solid #86A9DF;
    border-radius: 8px;
    padding: 8px 12px;
    color: #2E2E32;
    font-weight: 600;
}
QLabel#pendingLabel {
    background: #FFF0BF;
    color: #6B5200;
    border-radius: 6px;
    padding: 4px 12px;
    font-weight: 600;
}
QLabel#errorLabel {
    color: #B02A2A;
    background: transparent;
}
QLabel#countLabel {
    color: #5D5D61;
}

/* ---------- Таблиці ---------- */
QTableView {
    background: #F6FAFD;
    alternate-background-color: #EAF2F9;
    border: 1px solid #C6D6E4;
    border-radius: 10px;
    gridline-color: #DCE7F1;
    selection-background-color: #C4D8F2;
    selection-color: #2E2E32;
}
QHeaderView::section {
    background: #E4EFF7;
    color: #2E2E32;
    padding: 6px 8px;
    border: none;
    border-bottom: 1px solid #C6D6E4;
    border-right: 1px solid #C6D6E4;
    font-weight: 600;
}
QTableCornerButton::section {
    background: #E4EFF7;
    border: none;
}

/* ---------- Поля введення ---------- */
QLineEdit, QComboBox, QDateEdit, QSpinBox, QDoubleSpinBox {
    background: #F6FAFD;
    border: 2px solid #A9BED3;
    border-radius: 6px;
    padding: 4px 8px;
    min-height: 20px;
    selection-background-color: #86A9DF;
    selection-color: #1E1E22;
}
QLineEdit:focus, QComboBox:focus, QDateEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus {
    border: 2px solid #86A9DF;
}
QLineEdit:disabled, QComboBox:disabled, QDateEdit:disabled {
    background: #E4EFF7;
    color: #8A8A90;
}
QTableView QLineEdit, QTableView QComboBox, QTableView QDateEdit,
QTableView QSpinBox, QTableView QDoubleSpinBox {
    border-radius: 0;
    padding: 0 4px;
    min-height: 0;
}
QComboBox QAbstractItemView {
    background: #F6FAFD;
    selection-background-color: #C4D8F2;
    selection-color: #2E2E32;
    outline: none;
}
QCheckBox {
    spacing: 8px;
    color: #5D5D61;
}
QCheckBox::indicator:checked {
    background: #69CC69;
    border: 1px solid #4DB34D;
    border-radius: 3px;
}

/* ---------- Кнопки ---------- */
QPushButton {
    background: #F6FAFD;
    border: 1px solid #A9BED3;
    border-radius: 8px;
    padding: 8px 16px;
    color: #2E2E32;
}
QPushButton:hover {
    background: #D5E3F5;
}
QPushButton:pressed {
    background: #C4D8F2;
}
QPushButton:disabled {
    color: #8A8A90;
    background: #EAF2F9;
    border-color: #D3E0EC;
}
QPushButton[kind="primary"] {
    background: #86A9DF;
    border-color: #86A9DF;
    color: #1E1E22;
    font-weight: 600;
}
QPushButton[kind="primary"]:hover {
    background: #6F95D2;
    border-color: #6F95D2;
}
QPushButton[kind="primary"]:pressed {
    background: #5F86C6;
}
QPushButton[kind="primary"]:disabled {
    background: #C9D9F0;
    border-color: #C9D9F0;
    color: #8A8A90;
}
QPushButton[kind="success"], QPushButton[kind="accent"] {
    background: #69CC69;
    border-color: #69CC69;
    color: #1E1E22;
    font-weight: 600;
}
QPushButton[kind="success"]:hover, QPushButton[kind="accent"]:hover {
    background: #52B852;
    border-color: #52B852;
}
QPushButton[kind="success"]:disabled {
    background: #EAF2F9;
    border-color: #D3E0EC;
    color: #8A8A90;
    font-weight: normal;
}
QPushButton[kind="danger"] {
    color: #B02A2A;
    border-color: #E3A3A3;
}
QPushButton[kind="danger"]:hover {
    background: #FBE4E4;
}
QPushButton[kind="danger"]:disabled {
    color: #8A8A90;
    border-color: #D3E0EC;
}

/* ---------- Вікно входу ---------- */
QFrame#loginHeader {
    background: #69CC69;
}
QLabel#loginTitle {
    color: #1E1E22;
    font-size: 14pt;
    font-weight: 700;
    background: transparent;
}
QLabel#loginSubtitle {
    color: #14361A;
    font-size: 15pt;
    font-weight: 600;
    background: transparent;
}
QLabel#loginPrompt {
    color: #FFFFFF;
    font-size: 14pt;
    font-weight: 700;
    background: transparent;
}
/* ---------- Інше ---------- */
QMenuBar {
    background: #F6FAFD;
    border-bottom: 1px solid #C6D6E4;
}
QMenuBar::item:selected {
    background: #D5E3F5;
}
QMenu {
    background: #F6FAFD;
    border: 1px solid #C6D6E4;
}
QMenu::item:selected {
    background: #C4D8F2;
    color: #2E2E32;
}
QStatusBar {
    background: #F6FAFD;
    color: #5D5D61;
    border-top: 1px solid #C6D6E4;
}
QScrollBar:vertical {
    background: transparent;
    width: 12px;
}
QScrollBar::handle:vertical {
    background: #A9BED3;
    border-radius: 5px;
    min-height: 30px;
}
QScrollBar:horizontal {
    background: transparent;
    height: 12px;
}
QScrollBar::handle:horizontal {
    background: #A9BED3;
    border-radius: 5px;
    min-width: 30px;
}
QScrollBar::add-line, QScrollBar::sub-line {
    width: 0;
    height: 0;
}
)QSS";

} // namespace

void Style::apply(QApplication &app)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    // Не підлаштовуватися під темну тему Windows: оформлення розраховане на світлу.
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
#endif
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette p;
    p.setColor(QPalette::Window, QColor(0xE4, 0xEF, 0xF7));
    p.setColor(QPalette::WindowText, QColor(0x2E, 0x2E, 0x32));
    p.setColor(QPalette::Base, QColor(0xF6, 0xFA, 0xFD));
    p.setColor(QPalette::AlternateBase, QColor(0xEA, 0xF2, 0xF9));
    p.setColor(QPalette::Text, QColor(0x2E, 0x2E, 0x32));
    p.setColor(QPalette::Button, QColor(0xF6, 0xFA, 0xFD));
    p.setColor(QPalette::ButtonText, QColor(0x2E, 0x2E, 0x32));
    p.setColor(QPalette::Highlight, QColor(0x86, 0xA9, 0xDF));
    p.setColor(QPalette::HighlightedText, QColor(0x1E, 0x1E, 0x22));
    p.setColor(QPalette::ToolTipBase, QColor(0x2E, 0x2E, 0x32));
    p.setColor(QPalette::ToolTipText, Qt::white);
    p.setColor(QPalette::PlaceholderText, QColor(0x8A, 0x8A, 0x90));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(0x8A, 0x8A, 0x90));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x8A, 0x8A, 0x90));
    app.setPalette(p);

    QFont font(QStringLiteral("Segoe UI"));
    font.setPointSizeF(10);
    app.setFont(font);

    app.setStyleSheet(QString::fromUtf8(kStyleSheet));
    app.setWindowIcon(appIcon());
}

void Style::setKind(QPushButton *button, const QString &kind)
{
    button->setProperty("kind", kind);
    button->style()->unpolish(button);
    button->style()->polish(button);
}

QLabel *Style::logoLabel(int width, QWidget *parent)
{
    auto *label = new QLabel(parent);
    label->setObjectName(QStringLiteral("logo"));
    label->setStyleSheet(QStringLiteral("background: transparent;"));
    label->setToolTip(QStringLiteral("ЖК «Паркові Озера»"));

    const QPixmap source(QStringLiteral(":/img/logo.png"));
    if (source.isNull()) {                      // логотип не знайдено — текстова заглушка
        label->setText(QStringLiteral("ЖК «Паркові Озера»"));
        label->setStyleSheet(QStringLiteral("color: #FFFFFF; font-weight: 700; font-size: 13pt; background: transparent;"));
        return label;
    }
    const qreal dpr = parent ? parent->devicePixelRatioF() : qApp->devicePixelRatio();
    const int height = qRound(double(width) * source.height() / source.width());
    QPixmap scaled = source.scaled(QSize(qRound(width * dpr), qRound(height * dpr)),
                                   Qt::KeepAspectRatio, Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(dpr);
    label->setPixmap(scaled);
    label->setFixedSize(width, height);
    return label;
}

QIcon Style::appIcon()
{
    return QIcon(QStringLiteral(":/img/icon.png"));
}
