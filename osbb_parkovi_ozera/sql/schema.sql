-- =====================================================================
-- Система управління житловим фондом ОСББ — ЖК «Паркові Озера»
-- Схема бази даних (SQLite). Вбудована в програму (ресурс :/sql/schema.sql).
--
-- Таблиці (8):
--   матеріали            — склад матеріалів (ціна, залишок)
--   види_робіт           — довідник видів робіт і послуг + кошторисна норма матеріалу
--   персонал_підрядники  — штатні працівники та підрядні організації
--   квартири             — квартири та власники
--   заявки               — обʼєкти / заявки на ремонт (занесені диспетчером)
--   списання_матеріалів  — облік списання матеріалів по заявках
--   доплати              — тарифи доплат (платні виклики, спецбригади)
--   оплати               — двостороння модель розрахунків:
--                          «Загальний фонд» і «Індивідуальний» рахунок квартири
--
-- Рішення:
--   * борг і сума — обчислювані поля (GENERATED ... VIRTUAL);
--   * залишок матеріалів змінюють тригери при списанні (CHECK не дає піти в мінус);
--   * CHECK-обмеження мають імена: програма показує по них зрозумілі повідомлення;
--   * FK: RESTRICT для довідників, CASCADE для зв'язаних проводок, SET NULL — де
--     запис може існувати без посилання.
-- =====================================================================

CREATE TABLE IF NOT EXISTS матеріали (
    код      INTEGER PRIMARY KEY AUTOINCREMENT,
    назва    TEXT NOT NULL CONSTRAINT uq_mat_name UNIQUE,
    одиниця  TEXT NOT NULL,
    ціна     REAL NOT NULL CONSTRAINT ck_mat_price CHECK (ціна >= 0),
    залишок  REAL NOT NULL DEFAULT 0 CONSTRAINT ck_mat_stock CHECK (залишок >= 0)
);

CREATE TABLE IF NOT EXISTS види_робіт (
    код                  INTEGER PRIMARY KEY AUTOINCREMENT,
    вид_роботи           TEXT NOT NULL CONSTRAINT uq_work_name UNIQUE,
    вартість             REAL NOT NULL CONSTRAINT ck_work_cost CHECK (вартість >= 0),
    кількість_робітників INTEGER NOT NULL CONSTRAINT ck_work_workers CHECK (кількість_робітників > 0),
    тривалість           INTEGER NOT NULL CONSTRAINT ck_work_duration CHECK (тривалість > 0),
    матеріал             INTEGER REFERENCES матеріали (код)
                                 ON UPDATE CASCADE ON DELETE SET NULL,
    норма_матеріалу      REAL CONSTRAINT ck_work_norm CHECK (норма_матеріалу IS NULL OR норма_матеріалу >= 0)
);

CREATE TABLE IF NOT EXISTS персонал_підрядники (
    код     INTEGER PRIMARY KEY AUTOINCREMENT,
    ПІБ     TEXT NOT NULL,
    тип     TEXT NOT NULL DEFAULT 'Штатний'
                 CONSTRAINT ck_staff_type CHECK (тип IN ('Штатний', 'Підрядник')),
    посада  TEXT NOT NULL,
    телефон TEXT,
    адреса  TEXT,
    паспорт TEXT CONSTRAINT uq_staff_passport UNIQUE,
    стаж    INTEGER NOT NULL DEFAULT 0 CONSTRAINT ck_staff_experience CHECK (стаж >= 0)
);

CREATE TABLE IF NOT EXISTS квартири (
    код      INTEGER PRIMARY KEY AUTOINCREMENT,
    корпус   TEXT NOT NULL,
    під_їзд  INTEGER NOT NULL CONSTRAINT ck_flat_entrance CHECK (під_їзд > 0),
    номер    INTEGER NOT NULL CONSTRAINT ck_flat_number CHECK (номер > 0),
    площа    REAL CONSTRAINT ck_flat_area CHECK (площа IS NULL OR площа > 0),
    власник  TEXT NOT NULL,
    телефон  TEXT,
    CONSTRAINT uq_flat UNIQUE (корпус, номер)
);

CREATE TABLE IF NOT EXISTS заявки (
    код             INTEGER PRIMARY KEY AUTOINCREMENT,
    дата_заявки     TEXT NOT NULL CONSTRAINT ck_req_date CHECK (дата_заявки GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]' AND date(дата_заявки) IS дата_заявки),
    джерело         TEXT NOT NULL DEFAULT 'Telegram-чат'
                         CONSTRAINT ck_req_source CHECK (джерело IN ('Telegram-чат', 'Телефон', 'Особисто', 'Плановий огляд')),
    тип_локації     TEXT NOT NULL
                         CONSTRAINT ck_req_location CHECK (тип_локації IN ('Загальнобудинкове майно', 'Під’їзд', 'Прибудинкова територія', 'Квартира')),
    місце           TEXT NOT NULL,
    квартира        INTEGER REFERENCES квартири (код)
                            ON UPDATE CASCADE ON DELETE RESTRICT,
    опис            TEXT NOT NULL,
    пріоритет       TEXT NOT NULL DEFAULT 'Планова'
                         CONSTRAINT ck_req_priority CHECK (пріоритет IN ('Аварійна', 'Термінова', 'Планова')),
    статус          TEXT NOT NULL DEFAULT 'Нова'
                         CONSTRAINT ck_req_status CHECK (статус IN ('Нова', 'Призначена', 'В роботі', 'Виконана', 'Скасована')),
    вид_роботи      INTEGER NOT NULL REFERENCES види_робіт (код)
                            ON UPDATE CASCADE ON DELETE RESTRICT,
    виконавець      INTEGER REFERENCES персонал_підрядники (код)
                            ON UPDATE CASCADE ON DELETE SET NULL,
    дата_початку    TEXT CONSTRAINT ck_req_start CHECK (дата_початку IS NULL OR (дата_початку GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]' AND date(дата_початку) IS дата_початку)),
    дата_закінчення TEXT CONSTRAINT ck_req_end CHECK (дата_закінчення IS NULL OR (дата_закінчення GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]' AND date(дата_закінчення) IS дата_закінчення)),
    CONSTRAINT ck_req_period CHECK (дата_закінчення IS NULL OR (дата_початку IS NOT NULL AND дата_закінчення >= дата_початку)),
    CONSTRAINT ck_req_flat CHECK (тип_локації <> 'Квартира' OR квартира IS NOT NULL)
);

CREATE TABLE IF NOT EXISTS списання_матеріалів (
    код       INTEGER PRIMARY KEY AUTOINCREMENT,
    дата      TEXT NOT NULL CONSTRAINT ck_write_date CHECK (дата GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]' AND date(дата) IS дата),
    заявка    INTEGER NOT NULL REFERENCES заявки (код)
                      ON UPDATE CASCADE ON DELETE CASCADE,
    матеріал  INTEGER NOT NULL REFERENCES матеріали (код)
                      ON UPDATE CASCADE ON DELETE RESTRICT,
    кількість REAL NOT NULL CONSTRAINT ck_write_qty CHECK (кількість > 0),
    ціна      REAL NOT NULL CONSTRAINT ck_write_price CHECK (ціна >= 0),
    джерело   TEXT NOT NULL DEFAULT 'Загальний фонд'
                   CONSTRAINT ck_write_source CHECK (джерело IN ('Загальний фонд', 'Власник квартири')),
    сума      REAL GENERATED ALWAYS AS (ROUND(кількість * ціна, 2)) VIRTUAL
);

CREATE TABLE IF NOT EXISTS доплати (
    код   INTEGER PRIMARY KEY AUTOINCREMENT,
    назва TEXT NOT NULL CONSTRAINT uq_surcharge_name UNIQUE,
    сума  REAL NOT NULL CONSTRAINT ck_surcharge_sum CHECK (сума >= 0),
    опис  TEXT
);

CREATE TABLE IF NOT EXISTS оплати (
    код         INTEGER PRIMARY KEY AUTOINCREMENT,
    дата        TEXT NOT NULL CONSTRAINT ck_pay_date CHECK (дата GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]' AND date(дата) IS дата),
    рахунок     TEXT NOT NULL DEFAULT 'Індивідуальний'
                     CONSTRAINT ck_pay_account CHECK (рахунок IN ('Загальний фонд', 'Індивідуальний')),
    квартира    INTEGER REFERENCES квартири (код)
                        ON UPDATE CASCADE ON DELETE RESTRICT,
    заявка      INTEGER REFERENCES заявки (код)
                        ON UPDATE CASCADE ON DELETE SET NULL,
    призначення TEXT NOT NULL,
    нараховано  REAL NOT NULL CONSTRAINT ck_pay_charged CHECK (нараховано >= 0),
    тип_доплати INTEGER REFERENCES доплати (код)
                        ON UPDATE CASCADE ON DELETE SET NULL,
    доплата     REAL NOT NULL DEFAULT 0 CONSTRAINT ck_pay_surcharge CHECK (доплата >= 0),
    сплачено    REAL NOT NULL DEFAULT 0 CONSTRAINT ck_pay_paid CHECK (сплачено >= 0),
    борг        REAL GENERATED ALWAYS AS (ROUND(нараховано + доплата - сплачено, 2)) VIRTUAL,
    CONSTRAINT ck_pay_limit CHECK (сплачено <= нараховано + доплата),
    CONSTRAINT ck_pay_flat CHECK (рахунок = 'Загальний фонд' OR квартира IS NOT NULL)
);

-- Залишок матеріалів змінюється автоматично при списанні.
-- Якщо матеріалу не вистачає, спрацьовує CHECK ck_mat_stock і списання скасовується.
CREATE TRIGGER IF NOT EXISTS tr_write_insert AFTER INSERT ON списання_матеріалів
BEGIN
    UPDATE матеріали SET залишок = ROUND(залишок - NEW.кількість, 3) WHERE код = NEW.матеріал;
END;

CREATE TRIGGER IF NOT EXISTS tr_write_update AFTER UPDATE OF кількість, матеріал ON списання_матеріалів
BEGIN
    UPDATE матеріали SET залишок = ROUND(залишок + OLD.кількість, 3) WHERE код = OLD.матеріал;
    UPDATE матеріали SET залишок = ROUND(залишок - NEW.кількість, 3) WHERE код = NEW.матеріал;
END;

CREATE TRIGGER IF NOT EXISTS tr_write_delete AFTER DELETE ON списання_матеріалів
BEGIN
    UPDATE матеріали SET залишок = ROUND(залишок + OLD.кількість, 3) WHERE код = OLD.матеріал;
END;

-- Індекси для зовнішніх ключів і періодів
CREATE INDEX IF NOT EXISTS ix_види_матеріал    ON види_робіт (матеріал);
CREATE INDEX IF NOT EXISTS ix_заявки_дата      ON заявки (дата_заявки);
CREATE INDEX IF NOT EXISTS ix_заявки_статус    ON заявки (статус);
CREATE INDEX IF NOT EXISTS ix_заявки_квартира  ON заявки (квартира);
CREATE INDEX IF NOT EXISTS ix_заявки_вид       ON заявки (вид_роботи);
CREATE INDEX IF NOT EXISTS ix_заявки_виконавець ON заявки (виконавець);
CREATE INDEX IF NOT EXISTS ix_списання_заявка  ON списання_матеріалів (заявка);
CREATE INDEX IF NOT EXISTS ix_списання_матеріал ON списання_матеріалів (матеріал);
CREATE INDEX IF NOT EXISTS ix_списання_дата    ON списання_матеріалів (дата);
CREATE INDEX IF NOT EXISTS ix_оплати_дата      ON оплати (дата);
CREATE INDEX IF NOT EXISTS ix_оплати_квартира  ON оплати (квартира);
CREATE INDEX IF NOT EXISTS ix_оплати_заявка    ON оплати (заявка);
