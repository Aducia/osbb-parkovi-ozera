#ifndef STYLE_H
#define STYLE_H

#include <QIcon>

class QApplication;
class QLabel;
class QPushButton;
class QString;
class QWidget;

// Оформлення програми у фірмових кольорах ЖК «Паркові Озера»:
//   зелений #69CC69, білий (фон) #E4EFF7, сірий (текст) #5D5D61, блакитний (акценти) #86A9DF.
namespace Style {
void apply(QApplication &app);

// Вид кнопки: "primary" (блакитна), "success" / "accent" (зелена), "danger" (червона), "" (звичайна).
void setKind(QPushButton *button, const QString &kind);

// Логотип ЖК (білий, на прозорому тлі) заданої ширини; призначений для зеленого фону.
QLabel *logoLabel(int width, QWidget *parent);

// Піктограма застосунку
QIcon appIcon();
}

#endif // STYLE_H
