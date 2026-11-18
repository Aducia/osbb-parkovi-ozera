#include "mainwindow.h"

#include "auth.h"
#include "bureautablemodel.h"
#include "dashboardpage.h"
#include "database.h"
#include "invoicedialog.h"
#include "reportspage.h"
#include "style.h"
#include "tablepage.h"

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>
#include <cmath>

namespace {

// Довідники для випадних списків: код, коротка назва [, повна назва [, додаткове значення]]
const char *kFlatLookup =
    "SELECT код, корпус || ', кв. ' || номер, корпус || ', кв. ' || номер || ' — ' || власник "
    "FROM квартири ORDER BY корпус, номер";
const char *kStaffLookup =
    "SELECT код, ПІБ, ПІБ || ' — ' || посада FROM персонал_підрядники ORDER BY ПІБ";
const char *kWorkLookup =
    "SELECT код, вид_роботи FROM види_робіт ORDER BY вид_роботи";
// додаткове значення — ціна за одиницю (підставляється у списання)
const char *kMaterialLookup =
    "SELECT код, назва || ', ' || одиниця, "
    "назва || ' (' || одиниця || ', залишок ' || залишок || ')', ціна FROM матеріали ORDER BY назва";
const char *kRequestLookup =
    "SELECT z.код, '№' || z.код || ' — ' || v.вид_роботи, "
    "'№' || z.код || ' — ' || v.вид_роботи || ', ' || z.місце "
    "FROM заявки z JOIN види_робіт v ON v.код = z.вид_роботи ORDER BY z.код DESC";
// додаткове значення — сума доплати
const char *kSurchargeLookup =
    "SELECT код, назва, назва || ' — ' || сума || ' грн', сума FROM доплати ORDER BY назва";

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("ЖК «Паркові Озера» — управління житловим фондом ОСББ"));
    resize(1380, 860);
    setMinimumSize(1100, 680);

    auto *central = new QWidget(this);
    auto *rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(buildSidebar());

    // Права частина: заголовок + сторінки
    auto *content = new QWidget(central);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    auto *header = new QFrame(content);
    header->setObjectName(QStringLiteral("pageHeader"));
    auto *headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(24, 16, 24, 14);
    headerLayout->setSpacing(2);
    title_ = new QLabel(header);
    title_->setObjectName(QStringLiteral("pageTitle"));
    subtitle_ = new QLabel(header);
    subtitle_->setObjectName(QStringLiteral("pageSubtitle"));
    headerLayout->addWidget(title_);
    headerLayout->addWidget(subtitle_);
    contentLayout->addWidget(header);

    stack_ = new QStackedWidget(content);
    contentLayout->addWidget(stack_, 1);
    rootLayout->addWidget(content, 1);
    setCentralWidget(central);

    // ---- Сторінки
    dashboard_ = new DashboardPage(stack_);
    addPage(QStringLiteral("ОГЛЯД"), QStringLiteral("Головна"),
            QStringLiteral("Ключові показники будинку на сьогодні"), dashboard_);

    TablePage *requests = addTablePage(
        QStringLiteral("РОБОТА"), QStringLiteral("Об’єкти / Заявки на ремонт"),
        QStringLiteral("Заявки з Telegram-чатів мешканців: місце (майно, під’їзд, територія, квартира), пріоритет, "
                       "виконавець, терміни"),
        buildRequests());
    requests->setSort(0, Qt::DescendingOrder);

    TablePage *writeOffs = addTablePage(
        QStringLiteral("РОБОТА"), QStringLiteral("Списання матеріалів"),
        QStringLiteral("Облік матеріалів, витрачених на заявки: за рахунок загального фонду або власника квартири"),
        buildWriteOffs());
    writeOffs->setSort(0, Qt::DescendingOrder);

    TablePage *payments = addTablePage(
        QStringLiteral("ФІНАНСИ"), QStringLiteral("Оплати"),
        QStringLiteral("Загальний фонд (цільові внески) та індивідуальні рахунки квартир; борг рахується автоматично"),
        buildPayments());
    payments->setSort(0, Qt::DescendingOrder);
    payments->setExtraAction(
        QStringLiteral("Рахунок власнику"),
        QStringLiteral("Сформувати індивідуальний рахунок власнику квартири за заявкою"),
        [this] {
            InvoiceDialog dialog(this);
            dialog.exec();
            return dialog.created();
        });

    addTablePage(QStringLiteral("ФІНАНСИ"), QStringLiteral("Доплати та виклики"),
                 QStringLiteral("Тарифи платних викликів і послуг підрядників: сума підставляється в оплату автоматично"),
                 buildSurcharges());

    addTablePage(QStringLiteral("ДОВІДНИКИ"), QStringLiteral("Персонал та підрядники"),
                 QStringLiteral("Штат ЖК (техмайстри, двірники, садівники, ландшафтний дизайнер, правління) і підрядні організації"),
                 buildStaff());
    addTablePage(QStringLiteral("ДОВІДНИКИ"), QStringLiteral("Квартири та власники"),
                 QStringLiteral("Квартири ЖК і їхні власники: для заявок по квартирах та індивідуальних рахунків"),
                 buildFlats());
    addTablePage(QStringLiteral("ДОВІДНИКИ"), QStringLiteral("Матеріали (склад)"),
                 QStringLiteral("Матеріали, ціни й залишок; залишок зменшується автоматично під час списання"),
                 buildMaterials());
    addTablePage(QStringLiteral("ДОВІДНИКИ"), QStringLiteral("Види робіт та послуг"),
                 QStringLiteral("Довідник робіт і послуг із вартістю та кошторисною нормою матеріалу"),
                 buildWorkTypes());

    addPage(QStringLiteral("АНАЛІТИКА"), QStringLiteral("Звіти"),
            QStringLiteral("Звіти за період з підсумками та експортом у CSV"), new ReportsPage(stack_));

    buildMenu();

    auto *userInfo = new QLabel(QStringLiteral("Користувач: %1 · %2")
                                    .arg(Auth::currentUser(), Auth::roleName(Auth::currentRole())), this);
    statusBar()->addWidget(userInfo);
    statusBar()->addPermanentWidget(new QLabel(Database::displayName(), this));

    showPage(0);
}

// ---------------------------------------------------------------- побудова інтерфейсу

QWidget *MainWindow::buildSidebar()
{
    auto *sidebar = new QFrame(this);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(262);

    auto *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(14, 16, 14, 14);
    layout->setSpacing(4);

    // Логотип ЖК — лівий верхній кут
    layout->addWidget(Style::logoLabel(210, sidebar), 0, Qt::AlignLeft | Qt::AlignTop);
    auto *appSubtitle = new QLabel(QStringLiteral("Система управління житловим фондом ОСББ"), sidebar);
    appSubtitle->setObjectName(QStringLiteral("appSubtitle"));
    appSubtitle->setWordWrap(true);
    layout->addWidget(appSubtitle);
    layout->addSpacing(6);

    // Навігація (кнопки додаються в addPage); при малій висоті вікна прокручується
    nav_ = new QButtonGroup(this);
    nav_->setExclusive(true);
    connect(nav_, &QButtonGroup::idClicked, this, &MainWindow::showPage);

    auto *scroll = new QScrollArea(sidebar);
    scroll->setObjectName(QStringLiteral("navScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);
    auto *container = new QWidget(scroll);
    container->setObjectName(QStringLiteral("navContainer"));
    navLayout_ = new QVBoxLayout(container);
    navLayout_->setContentsMargins(0, 0, 0, 0);
    navLayout_->setSpacing(2);
    navLayout_->addStretch();
    scroll->setWidget(container);
    layout->addWidget(scroll, 1);

    // Користувач
    auto *userBox = new QFrame(sidebar);
    userBox->setObjectName(QStringLiteral("userBox"));
    auto *userLayout = new QVBoxLayout(userBox);
    userLayout->setContentsMargins(12, 10, 12, 12);
    userLayout->setSpacing(6);
    auto *userName = new QLabel(Auth::currentUser(), userBox);
    userName->setObjectName(QStringLiteral("userName"));
    auto *userRole = new QLabel(Auth::roleName(Auth::currentRole()), userBox);
    userRole->setObjectName(QStringLiteral("userRole"));
    userLayout->addWidget(userName);
    userLayout->addWidget(userRole);

    auto *buttons = new QHBoxLayout();
    buttons->setSpacing(6);
    auto *passwordButton = new QPushButton(QStringLiteral("Пароль"), userBox);
    passwordButton->setObjectName(QStringLiteral("sideAction"));
    passwordButton->setToolTip(QStringLiteral("Змінити пароль"));
    auto *logoutButton = new QPushButton(QStringLiteral("Вийти"), userBox);
    logoutButton->setObjectName(QStringLiteral("sideAction"));
    logoutButton->setToolTip(QStringLiteral("Вийти з облікового запису"));
    for (QPushButton *b : {passwordButton, logoutButton}) {
        b->setFocusPolicy(Qt::NoFocus);
        b->setCursor(Qt::PointingHandCursor);
        buttons->addWidget(b);
    }
    userLayout->addLayout(buttons);
    layout->addWidget(userBox);

    connect(passwordButton, &QPushButton::clicked, this, &MainWindow::changePassword);
    connect(logoutButton, &QPushButton::clicked, this, &MainWindow::logout);
    return sidebar;
}

void MainWindow::addPage(const QString &section, const QString &title, const QString &subtitle,
                         QWidget *page)
{
    QWidget *container = navLayout_->parentWidget();

    static const char *kLastSection = "lastSection";
    if (navLayout_->property(kLastSection).toString() != section) {
        auto *label = new QLabel(section, container);
        label->setObjectName(QStringLiteral("navSection"));
        navLayout_->insertWidget(navLayout_->count() - 1, label);   // перед розтяжкою
        navLayout_->setProperty(kLastSection, section);
    }

    const int index = stack_->addWidget(page);
    titles_ << title;
    subtitles_ << subtitle;

    auto *button = new QPushButton(title, container);
    button->setObjectName(QStringLiteral("navButton"));
    button->setCheckable(true);
    button->setFocusPolicy(Qt::NoFocus);
    button->setCursor(Qt::PointingHandCursor);
    nav_->addButton(button, index);
    navLayout_->insertWidget(navLayout_->count() - 1, button);
}

TablePage *MainWindow::addTablePage(const QString &section, const QString &title,
                                    const QString &subtitle, BureauTableModel *model)
{
    auto *page = new TablePage(model, title, stack_);
    tablePages_ << page;
    addPage(section, title, subtitle, page);
    return page;
}

void MainWindow::buildMenu()
{
    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("&Файл"));
    QAction *password = fileMenu->addAction(QStringLiteral("Змінити пароль…"));
    connect(password, &QAction::triggered, this, &MainWindow::changePassword);
    QAction *logoutAction = fileMenu->addAction(QStringLiteral("Вийти з облікового запису"));
    connect(logoutAction, &QAction::triggered, this, &MainWindow::logout);
    fileMenu->addSeparator();
    QAction *quit = fileMenu->addAction(QStringLiteral("Вихід"));
    quit->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
    connect(quit, &QAction::triggered, this, &QWidget::close);

    QMenu *goMenu = menuBar()->addMenu(QStringLiteral("&Перейти"));
    for (int i = 0; i < titles_.size(); ++i) {
        QAction *a = goMenu->addAction(titles_.at(i));
        if (i < 9)
            a->setShortcut(QKeySequence(Qt::CTRL | static_cast<Qt::Key>(Qt::Key_1 + i)));
        connect(a, &QAction::triggered, this, [this, i] { showPage(i); });
    }

    QMenu *helpMenu = menuBar()->addMenu(QStringLiteral("&Довідка"));
    QAction *aboutAction = helpMenu->addAction(QStringLiteral("Про програму"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::about);
    QAction *aboutQt = helpMenu->addAction(QStringLiteral("Про Qt"));
    connect(aboutQt, &QAction::triggered, qApp, &QApplication::aboutQt);
}

void MainWindow::showPage(int index)
{
    if (index < 0 || index >= stack_->count())
        return;
    stack_->setCurrentIndex(index);
    if (QAbstractButton *b = nav_->button(index))
        b->setChecked(true);
    title_->setText(titles_.value(index));
    subtitle_->setText(subtitles_.value(index));

    QWidget *page = stack_->widget(index);
    if (page == dashboard_)
        dashboard_->refresh();
    else if (auto *tablePage = qobject_cast<TablePage *>(page))
        tablePage->activate();
}

// ---------------------------------------------------------------- моделі таблиць

BureauTableModel *MainWindow::makeModel(const QString &table)
{
    auto *m = new BureauTableModel(table, this);
    m->setEditable(Auth::isAdmin());
    m->setReadOnlyFields({QStringLiteral("код")});
    m->setFieldKind(QStringLiteral("код"), BureauTableModel::Integer);
    return m;
}

BureauTableModel *MainWindow::buildRequests()
{
    auto *m = makeModel(QStringLiteral("заявки"));
    m->setFieldKind(QStringLiteral("дата_заявки"), BureauTableModel::Date);
    m->setFieldKind(QStringLiteral("дата_початку"), BureauTableModel::Date);
    m->setFieldKind(QStringLiteral("дата_закінчення"), BureauTableModel::Date);
    m->setChoices(QStringLiteral("джерело"),
                  {QStringLiteral("Telegram-чат"), QStringLiteral("Телефон"), QStringLiteral("Особисто"),
                   QStringLiteral("Плановий огляд")});
    m->setChoices(QStringLiteral("тип_локації"),
                  {QStringLiteral("Загальнобудинкове майно"), QStringLiteral("Під’їзд"),
                   QStringLiteral("Прибудинкова територія"), QStringLiteral("Квартира")});
    m->setChoices(QStringLiteral("пріоритет"),
                  {QStringLiteral("Аварійна"), QStringLiteral("Термінова"), QStringLiteral("Планова")});
    m->setChoices(QStringLiteral("статус"),
                  {QStringLiteral("Нова"), QStringLiteral("Призначена"), QStringLiteral("В роботі"),
                   QStringLiteral("Виконана"), QStringLiteral("Скасована")});
    m->setForeignKey(QStringLiteral("квартира"), QString::fromUtf8(kFlatLookup));
    m->setForeignKey(QStringLiteral("вид_роботи"), QString::fromUtf8(kWorkLookup));
    m->setForeignKey(QStringLiteral("виконавець"), QString::fromUtf8(kStaffLookup));
    m->setDefaultValue(QStringLiteral("джерело"), QStringLiteral("Telegram-чат"));
    m->setDefaultValue(QStringLiteral("пріоритет"), QStringLiteral("Планова"));
    m->setDefaultValue(QStringLiteral("статус"), QStringLiteral("Нова"));
    m->setRequiredFields({QStringLiteral("дата_заявки"), QStringLiteral("джерело"),
                          QStringLiteral("тип_локації"), QStringLiteral("місце"), QStringLiteral("опис"),
                          QStringLiteral("пріоритет"), QStringLiteral("статус"), QStringLiteral("вид_роботи")});
    return m;
}

BureauTableModel *MainWindow::buildWriteOffs()
{
    auto *m = makeModel(QStringLiteral("списання_матеріалів"));
    m->setFieldKind(QStringLiteral("дата"), BureauTableModel::Date);
    m->setForeignKey(QStringLiteral("заявка"), QString::fromUtf8(kRequestLookup));
    m->setForeignKey(QStringLiteral("матеріал"), QString::fromUtf8(kMaterialLookup));
    m->setFieldKind(QStringLiteral("кількість"), BureauTableModel::Quantity);
    m->setFieldKind(QStringLiteral("ціна"), BureauTableModel::Money);
    m->setChoices(QStringLiteral("джерело"),
                  {QStringLiteral("Загальний фонд"), QStringLiteral("Власник квартири")});
    m->setDefaultValue(QStringLiteral("джерело"), QStringLiteral("Загальний фонд"));
    // вибір матеріалу підставляє його ціну за одиницю
    m->setAutoFill(QStringLiteral("матеріал"), QStringLiteral("ціна"));
    // сума = кількість × ціна (обчислюється, не редагується)
    m->setComputed(QStringLiteral("сума"), {QStringLiteral("кількість"), QStringLiteral("ціна")},
                   [](const QList<double> &v) { return QVariant(std::round(v.value(0) * v.value(1) * 100.0) / 100.0); },
                   BureauTableModel::Money);
    m->setRequiredFields({QStringLiteral("дата"), QStringLiteral("заявка"), QStringLiteral("матеріал"),
                          QStringLiteral("кількість"), QStringLiteral("ціна"), QStringLiteral("джерело")});
    return m;
}

BureauTableModel *MainWindow::buildPayments()
{
    auto *m = makeModel(QStringLiteral("оплати"));
    m->setFieldKind(QStringLiteral("дата"), BureauTableModel::Date);
    m->setChoices(QStringLiteral("рахунок"),
                  {QStringLiteral("Загальний фонд"), QStringLiteral("Індивідуальний")});
    m->setDefaultValue(QStringLiteral("рахунок"), QStringLiteral("Індивідуальний"));
    m->setForeignKey(QStringLiteral("квартира"), QString::fromUtf8(kFlatLookup));
    m->setForeignKey(QStringLiteral("заявка"), QString::fromUtf8(kRequestLookup));
    m->setForeignKey(QStringLiteral("тип_доплати"), QString::fromUtf8(kSurchargeLookup));
    m->setFieldKind(QStringLiteral("нараховано"), BureauTableModel::Money);
    m->setFieldKind(QStringLiteral("доплата"), BureauTableModel::Money);
    m->setFieldKind(QStringLiteral("сплачено"), BureauTableModel::Money);
    // вибір типу доплати підставляє її суму
    m->setAutoFill(QStringLiteral("тип_доплати"), QStringLiteral("доплата"));
    // борг = нараховано + доплата − сплачено
    m->setComputed(QStringLiteral("борг"),
                   {QStringLiteral("нараховано"), QStringLiteral("доплата"), QStringLiteral("сплачено")},
                   [](const QList<double> &v) {
                       return QVariant(std::round((v.value(0) + v.value(1) - v.value(2)) * 100.0) / 100.0);
                   },
                   BureauTableModel::Money, /*positiveIsBad=*/true);
    m->setRequiredFields({QStringLiteral("дата"), QStringLiteral("рахунок"), QStringLiteral("призначення"),
                          QStringLiteral("нараховано"), QStringLiteral("доплата"), QStringLiteral("сплачено")});
    return m;
}

BureauTableModel *MainWindow::buildSurcharges()
{
    auto *m = makeModel(QStringLiteral("доплати"));
    m->setFieldKind(QStringLiteral("сума"), BureauTableModel::Money);
    m->setRequiredFields({QStringLiteral("назва"), QStringLiteral("сума")});
    return m;
}

BureauTableModel *MainWindow::buildStaff()
{
    auto *m = makeModel(QStringLiteral("персонал_підрядники"));
    m->setChoices(QStringLiteral("тип"), {QStringLiteral("Штатний"), QStringLiteral("Підрядник")});
    m->setDefaultValue(QStringLiteral("тип"), QStringLiteral("Штатний"));
    m->setFieldKind(QStringLiteral("стаж"), BureauTableModel::Integer);
    m->setRequiredFields({QStringLiteral("ПІБ"), QStringLiteral("тип"), QStringLiteral("посада"),
                          QStringLiteral("стаж")});
    return m;
}

BureauTableModel *MainWindow::buildFlats()
{
    auto *m = makeModel(QStringLiteral("квартири"));
    m->setFieldKind(QStringLiteral("під_їзд"), BureauTableModel::Integer);
    m->setFieldKind(QStringLiteral("номер"), BureauTableModel::Integer);
    m->setFieldKind(QStringLiteral("площа"), BureauTableModel::Quantity);
    m->setRequiredFields({QStringLiteral("корпус"), QStringLiteral("під_їзд"), QStringLiteral("номер"),
                          QStringLiteral("власник")});
    return m;
}

BureauTableModel *MainWindow::buildMaterials()
{
    auto *m = makeModel(QStringLiteral("матеріали"));
    m->setFieldKind(QStringLiteral("ціна"), BureauTableModel::Money);
    m->setFieldKind(QStringLiteral("залишок"), BureauTableModel::Quantity);
    m->setRequiredFields({QStringLiteral("назва"), QStringLiteral("одиниця"), QStringLiteral("ціна"),
                          QStringLiteral("залишок")});
    return m;
}

BureauTableModel *MainWindow::buildWorkTypes()
{
    auto *m = makeModel(QStringLiteral("види_робіт"));
    m->setFieldKind(QStringLiteral("вартість"), BureauTableModel::Money);
    m->setFieldKind(QStringLiteral("кількість_робітників"), BureauTableModel::Integer);
    m->setFieldKind(QStringLiteral("тривалість"), BureauTableModel::Integer);
    m->setForeignKey(QStringLiteral("матеріал"), QString::fromUtf8(kMaterialLookup));
    m->setFieldKind(QStringLiteral("норма_матеріалу"), BureauTableModel::Quantity);
    m->setRequiredFields({QStringLiteral("вид_роботи"), QStringLiteral("вартість"),
                          QStringLiteral("кількість_робітників"), QStringLiteral("тривалість")});
    return m;
}

// ---------------------------------------------------------------- дії

void MainWindow::changePassword()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Зміна пароля"));
    dialog.setMinimumWidth(380);

    auto *root = new QVBoxLayout(&dialog);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(12);
    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auto *oldEdit = new QLineEdit(&dialog);
    auto *newEdit = new QLineEdit(&dialog);
    auto *repeatEdit = new QLineEdit(&dialog);
    for (QLineEdit *e : {oldEdit, newEdit, repeatEdit})
        e->setEchoMode(QLineEdit::Password);
    form->addRow(QStringLiteral("Поточний пароль:"), oldEdit);
    form->addRow(QStringLiteral("Новий пароль:"), newEdit);
    form->addRow(QStringLiteral("Повторіть пароль:"), repeatEdit);
    root->addLayout(form);

    auto *error = new QLabel(&dialog);
    error->setObjectName(QStringLiteral("errorLabel"));
    error->setWordWrap(true);
    error->hide();
    root->addWidget(error);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch();
    auto *cancel = new QPushButton(QStringLiteral("Скасувати"), &dialog);
    auto *ok = new QPushButton(QStringLiteral("Змінити"), &dialog);
    Style::setKind(ok, QStringLiteral("primary"));
    ok->setDefault(true);
    buttons->addWidget(cancel);
    buttons->addWidget(ok);
    root->addLayout(buttons);

    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(ok, &QPushButton::clicked, &dialog, [&] {
        QString message;
        if (newEdit->text().size() < 4)
            message = QStringLiteral("Новий пароль має містити щонайменше 4 символи.");
        else if (newEdit->text() != repeatEdit->text())
            message = QStringLiteral("Нові паролі не збігаються.");
        else if (!Auth::changePassword(oldEdit->text(), newEdit->text()))
            message = QStringLiteral("Поточний пароль неправильний.");
        if (!message.isEmpty()) {
            error->setText(message);
            error->show();
            return;
        }
        dialog.accept();
    });

    if (dialog.exec() == QDialog::Accepted)
        QMessageBox::information(this, QStringLiteral("Зміна пароля"), QStringLiteral("Пароль змінено."));
}

void MainWindow::logout()
{
    logout_ = true;
    close();
}

void MainWindow::about()
{
    QMessageBox::about(this, QStringLiteral("Про програму"),
        QStringLiteral("<h3>ЖК «Паркові Озера»</h3>"
                       "<p>Система управління житловим фондом ОСББ — внутрішній інструмент для "
                       "диспетчерів, майстрів і правління.</p>"
                       "<p>Облік заявок на ремонт, персоналу й підрядників, квартир і власників, "
                       "матеріалів та їх списання, оплат (загальний фонд та індивідуальні рахунки "
                       "з доплатами); звіти за період з експортом у CSV; розмежування доступу "
                       "(адміністрація правління / перегляд).</p>"
                       "<p>Qt %1 · SQLite<br>База даних: %2</p>")
            .arg(QString::fromLatin1(qVersion()), Database::filePath().toHtmlEscaped()));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QList<TablePage *> dirty;
    QStringList names;
    for (TablePage *page : tablePages_) {
        if (page->hasPendingChanges()) {
            dirty << page;
            names << QStringLiteral("«") + page->title() + QStringLiteral("»");
        }
    }

    if (!dirty.isEmpty()) {
        const auto choice = TablePage::askPending(
            this, QStringLiteral("Є незбережені зміни в таблицях: %1.\nЗберегти їх перед виходом?")
                      .arg(names.join(QStringLiteral(", "))));
        if (choice == TablePage::CancelChoice) {
            logout_ = false;
            event->ignore();
            return;
        }
        for (TablePage *page : dirty) {
            if (choice == TablePage::DiscardChoice) {
                page->discardChanges();
            } else if (!page->saveChanges()) {
                logout_ = false;
                showPage(stack_->indexOf(page));
                event->ignore();
                return;
            }
        }
    }

    event->accept();
    emit closed();
}
