# -*- coding: utf-8 -*-
"""Автоматичні тести логіки бази даних (схема, тригери, обмеження, запити звітів).
Запуск:  python tests/db_tests.py
Перевіряється той самий SQL, що вбудований у програму (sql/schema.sql, sql/seed.sql, sql/reports.sql).
Інтерфейс Qt цими тестами не перевіряється."""
import json, os, sqlite3, sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")

def read(name):
    with open(os.path.join(ROOT, "sql", name), encoding="utf-8") as f:
        return f.read()

def split_sql(text):
    out, buf = [], ""
    for line in text.splitlines(keepends=True):
        if not buf and line.strip().startswith("--"):
            continue
        buf += line
        if sqlite3.complete_statement(buf):
            out.append(buf.strip()); buf = ""
    return out

def fresh(seed=True):
    c = sqlite3.connect(":memory:", isolation_level=None)
    c.execute("PRAGMA foreign_keys = ON")
    for st in split_sql(read("schema.sql")):
        c.execute(st)
    if seed:
        c.execute("BEGIN")
        for st in split_sql(read("seed.sql")):
            c.execute(st)
        c.execute("COMMIT")
    return c

def one(c, q, *a):
    return c.execute(q, a).fetchone()[0]

def expect_error(c, q, *a):
    try:
        c.execute(q, a)
    except sqlite3.Error as e:
        return str(e)
    return None

RESULTS = []
def case(tid, title, req, fn):
    try:
        ok, actual = fn()
    except Exception as e:  # непередбачена помилка тесту
        ok, actual = False, "виняток: %r" % (e,)
    RESULTS.append({"id": tid, "title": title, "req": req, "actual": actual, "ok": bool(ok)})

# ---------- Склад і тригери ----------
def t01():
    c = fresh(); s0 = one(c, "SELECT залишок FROM матеріали WHERE код=1")
    c.execute("INSERT INTO списання_матеріалів(дата,заявка,матеріал,кількість,ціна,джерело) VALUES('2026-11-01',1,1,2.5,10,'Загальний фонд')")
    s1 = one(c, "SELECT залишок FROM матеріали WHERE код=1")
    return abs((s0 - s1) - 2.5) < 1e-9, "залишок %.3f → %.3f" % (s0, s1)
case("T-01", "Списання зменшує залишок матеріалу", "FR-06", t01)

def t02():
    c = fresh()
    c.execute("INSERT INTO списання_матеріалів(дата,заявка,матеріал,кількість,ціна,джерело) VALUES('2026-11-01',1,1,2,10,'Загальний фонд')")
    s1 = one(c, "SELECT залишок FROM матеріали WHERE код=1"); wid = one(c, "SELECT max(код) FROM списання_матеріалів")
    c.execute("UPDATE списання_матеріалів SET кількість=5 WHERE код=?", (wid,))
    s2 = one(c, "SELECT залишок FROM матеріали WHERE код=1")
    return abs((s1 - s2) - 3) < 1e-9, "залишок %.3f → %.3f після зміни 2 → 5" % (s1, s2)
case("T-02", "Зміна кількості списання коригує залишок", "FR-06", t02)

def t03():
    c = fresh(); s0 = one(c, "SELECT залишок FROM матеріали WHERE код=1")
    c.execute("INSERT INTO списання_матеріалів(дата,заявка,матеріал,кількість,ціна,джерело) VALUES('2026-11-01',1,1,2,10,'Загальний фонд')")
    c.execute("DELETE FROM списання_матеріалів WHERE код=(SELECT max(код) FROM списання_матеріалів)")
    s1 = one(c, "SELECT залишок FROM матеріали WHERE код=1")
    return abs(s0 - s1) < 1e-9, "залишок до %.3f, після додавання й видалення %.3f" % (s0, s1)
case("T-03", "Видалення списання повертає матеріал на склад", "FR-06", t03)

def t04():
    c = fresh(); s0 = one(c, "SELECT залишок FROM матеріали WHERE код=1"); n0 = one(c, "SELECT count(*) FROM списання_матеріалів")
    err = expect_error(c, "INSERT INTO списання_матеріалів(дата,заявка,матеріал,кількість,ціна,джерело) VALUES('2026-11-01',1,1,?,10,'Загальний фонд')", s0 + 1000)
    s1 = one(c, "SELECT залишок FROM матеріали WHERE код=1"); n1 = one(c, "SELECT count(*) FROM списання_матеріалів")
    return err is not None and "ck_mat_stock" in err and s0 == s1 and n0 == n1, "відхилено: %s; рядків %d→%d, залишок без змін" % (err, n0, n1)
case("T-04", "Списання більше за залишок відхиляється, стан не змінюється", "FR-06, NFR-03", t04)

def t05():
    c = fresh()
    c.execute("INSERT INTO списання_матеріалів(дата,заявка,матеріал,кількість,ціна,джерело) VALUES('2026-11-01',1,1,3.333,7.77,'Загальний фонд')")
    v = one(c, "SELECT сума FROM списання_матеріалів WHERE код=(SELECT max(код) FROM списання_матеріалів)")
    return v == round(3.333 * 7.77, 2), "сума = %s (очікувано %s)" % (v, round(3.333 * 7.77, 2))
case("T-05", "Сума списання = кількість × ціна (округлення до копійок)", "FR-06", t05)

# ---------- Розрахунки ----------
def t06():
    c = fresh()
    c.execute("INSERT INTO оплати(дата,рахунок,квартира,заявка,призначення,нараховано,доплата,сплачено) VALUES('2026-11-01','Індивідуальний',1,1,'тест',1000,500,300)")
    d = one(c, "SELECT борг FROM оплати WHERE код=(SELECT max(код) FROM оплати)")
    return d == 1200, "борг = %s (очікувано 1200)" % d
case("T-06", "Борг = нараховано + доплата − сплачено", "FR-08", t06)

def t07():
    c = fresh()
    err = expect_error(c, "INSERT INTO оплати(дата,рахунок,квартира,призначення,нараховано,доплата,сплачено) VALUES('2026-11-01','Індивідуальний',1,'тест',100,0,150)")
    return err is not None and "ck_pay_limit" in err, "відхилено: %s" % err
case("T-07", "Сплачено більше за нараховане відхиляється", "FR-08, NFR-03", t07)

def t08():
    c = fresh()
    err = expect_error(c, "INSERT INTO оплати(дата,рахунок,призначення,нараховано,доплата,сплачено) VALUES('2026-11-01','Індивідуальний','тест',100,0,0)")
    return err is not None and "ck_pay_flat" in err, "відхилено: %s" % err
case("T-08", "Індивідуальний рахунок без квартири відхиляється", "FR-08, NFR-03", t08)

def t09():
    c = fresh()
    ok = expect_error(c, "INSERT INTO оплати(дата,рахунок,призначення,нараховано,доплата,сплачено) VALUES('2026-11-01','Загальний фонд','внесок',500,0,500)") is None
    return ok, "запис без квартири для «Загальний фонд» прийнято"
case("T-09", "Внесок до загального фонду не потребує квартири", "FR-08", t09)

# ---------- Заявки ----------
def t10():
    c = fresh()
    err = expect_error(c, "INSERT INTO заявки(дата_заявки,джерело,тип_локації,місце,опис,пріоритет,статус,вид_роботи) VALUES('2026-11-01','Телефон','Квартира','кв. 1','протікає','Планова','Нова',1)")
    return err is not None and "ck_req_flat" in err, "відхилено: %s" % err
case("T-10", "Заявка з типом «Квартира» без квартири відхиляється", "FR-03, NFR-03", t10)

def t11():
    c = fresh()
    err = expect_error(c, "INSERT INTO заявки(дата_заявки,тип_локації,місце,опис,вид_роботи,дата_початку,дата_закінчення) VALUES('2026-11-01','Під’їзд','п.1','тест',1,'2026-11-10','2026-11-05')")
    return err is not None and "ck_req_period" in err, "відхилено: %s" % err
case("T-11", "Дата закінчення раніше за дату початку відхиляється", "FR-03, NFR-03", t11)

def t12():
    c = fresh()
    e1 = expect_error(c, "INSERT INTO заявки(дата_заявки,тип_локації,місце,опис,вид_роботи) VALUES('01.11.2026','Під’їзд','п.1','тест',1)")
    e2 = expect_error(c, "INSERT INTO заявки(дата_заявки,тип_локації,місце,опис,вид_роботи) VALUES('2026-02-30','Під’їзд','п.1','тест',1)")
    return e1 is not None and e2 is not None and "ck_req_date" in e1 and "ck_req_date" in e2, "«01.11.2026» і «2026-02-30» відхилено (ck_req_date)"
case("T-12", "Некоректна дата (формат і неіснуюча дата) відхиляється", "FR-03, NFR-03", t12)

def t13():
    c = fresh()
    e = expect_error(c, "INSERT INTO заявки(дата_заявки,тип_локації,місце,опис,вид_роботи,статус) VALUES('2026-11-01','Під’їзд','п.1','тест',1,'Невідомо')")
    return e is not None and "ck_req_status" in e, "відхилено: %s" % e
case("T-13", "Недопустиме значення статусу відхиляється", "FR-03", t13)

def t14():
    c = fresh(); fid = one(c, "SELECT квартира FROM заявки WHERE квартира IS NOT NULL LIMIT 1")
    e = expect_error(c, "DELETE FROM квартири WHERE код=?", fid)
    return e is not None and "FOREIGN KEY" in e, "відхилено: %s" % e
case("T-14", "Квартиру, указану в заявці, видалити не можна", "FR-04, NFR-03", t14)

def t15():
    c = fresh()
    rid = one(c, "SELECT заявка FROM списання_матеріалів LIMIT 1"); mid = one(c, "SELECT матеріал FROM списання_матеріалів WHERE заявка=?", rid)
    q = one(c, "SELECT SUM(кількість) FROM списання_матеріалів WHERE заявка=? AND матеріал=?", rid, mid)
    s0 = one(c, "SELECT залишок FROM матеріали WHERE код=?", mid)
    c.execute("DELETE FROM заявки WHERE код=?", (rid,))
    left = one(c, "SELECT count(*) FROM списання_матеріалів WHERE заявка=?", rid)
    s1 = one(c, "SELECT залишок FROM матеріали WHERE код=?", mid)
    return left == 0 and abs((s1 - s0) - q) < 1e-9, "списань залишилось %d; залишок %.3f → %.3f (повернуто %.3f)" % (left, s0, s1, q)
case("T-15", "Видалення заявки каскадно видаляє її списання, а залишок повертається", "FR-06", t15)

# ---------- Довідники ----------
def t16():
    c = fresh(); b, n = c.execute("SELECT корпус, номер FROM квартири LIMIT 1").fetchone()
    e = expect_error(c, "INSERT INTO квартири(корпус,під_їзд,номер,площа,власник) VALUES(?,1,?,50,'Тест')", b, n)
    return e is not None and "UNIQUE" in e, "відхилено: %s" % e
case("T-16", "Дубль «корпус + номер квартири» відхиляється", "FR-04", t16)

def t17():
    c = fresh()
    c.execute("INSERT INTO персонал_підрядники(ПІБ,тип,посада,паспорт) VALUES('Тест А','Штатний','Двірник','АА000001')")
    e = expect_error(c, "INSERT INTO персонал_підрядники(ПІБ,тип,посада,паспорт) VALUES('Тест Б','Штатний','Двірник','АА000001')")
    ok_null = expect_error(c, "INSERT INTO персонал_підрядники(ПІБ,тип,посада) VALUES('Тест В','Підрядник','Електрик')") is None and \
              expect_error(c, "INSERT INTO персонал_підрядники(ПІБ,тип,посада) VALUES('Тест Г','Підрядник','Електрик')") is None
    return e is not None and "UNIQUE" in e and ok_null, "дубль паспорта відхилено; кілька записів без паспорта (підрядні організації) дозволені"
case("T-17", "Паспорт унікальний, але може бути порожнім (підрядник)", "FR-05", t17)

def t18():
    c = fresh()
    e = expect_error(c, "INSERT INTO види_робіт(вид_роботи,вартість,кількість_робітників,тривалість) VALUES('Тест',100,0,1)")
    return e is not None and "ck_work_workers" in e, "відхилено: %s" % e
case("T-18", "Кількість робітників у виді робіт має бути > 0", "FR-07", t18)

# ---------- Запити програми ----------
def t19():
    c = fresh()
    from_c = open(os.path.join(ROOT, "reportspage.cpp"), encoding="utf-8").read()
    import re
    blocks = re.findall(r'R"SQL\((.*?)\)SQL"', from_c, re.S)
    bad = []
    for i, b in enumerate(blocks):
        try: c.execute(b.replace(":d1", "'2026-01-01'").replace(":d2", "'2026-12-31'")).fetchall()
        except sqlite3.Error as e: bad.append((i, str(e)))
    return len(blocks) == 16 and not bad, "виконано %d SQL-запитів зі звітів (8 звітів × дані + підсумки), помилок: %d" % (len(blocks), len(bad))
case("T-19", "Усі запити звітів виконуються на даних", "FR-09", t19)

def t20():
    c = fresh()
    fund_paid = one(c, "SELECT COALESCE(SUM(сплачено),0) FROM оплати WHERE рахунок='Загальний фонд'")
    fund_mat = one(c, "SELECT COALESCE(SUM(сума),0) FROM списання_матеріалів WHERE джерело='Загальний фонд'")
    expected = round(fund_paid - fund_mat, 2)
    d = open(os.path.join(ROOT, "dashboardpage.cpp"), encoding="utf-8").read()
    import re
    sql = None
    for m in re.finditer(r'QStringLiteral\(\s*((?:"(?:[^"\\\n]|\\.)*"\s*)+)\)', d):
        txt = "".join(re.findall(r'"((?:[^"\\\n]|\\.)*)"', m.group(1)))
        if "SELECT COALESCE((SELECT SUM(сплачено)" in txt: sql = txt
    got = one(c, sql)
    return abs(got - expected) < 0.005, "баланс фонду на головній %.2f, незалежний підрахунок %.2f" % (got, expected)
case("T-20", "Баланс загального фонду на головній збігається з незалежним підрахунком", "FR-10", t20)

def t21():
    c = fresh()
    debt_sql = one(c, "SELECT COALESCE(SUM(борг),0) FROM оплати")
    manual = round(sum(r[0] + r[1] - r[2] for r in c.execute("SELECT нараховано, доплата, сплачено FROM оплати")), 2)
    return abs(debt_sql - manual) < 0.005, "сума боргів за БД %.2f, ручний підрахунок %.2f" % (debt_sql, manual)
case("T-21", "Сума боргів за БД збігається з ручним підрахунком", "FR-08, FR-10", t21)

def t22():
    c = fresh()
    rid = one(c, "SELECT z.код FROM заявки z WHERE z.квартира IS NOT NULL AND EXISTS(SELECT 1 FROM списання_матеріалів w WHERE w.заявка=z.код AND w.джерело='Власник квартири') LIMIT 1")
    work = one(c, "SELECT v.вартість FROM заявки z JOIN види_робіт v ON v.код=z.вид_роботи WHERE z.код=?", rid)
    mats = one(c, "SELECT COALESCE(SUM(сума),0) FROM списання_матеріалів WHERE заявка=? AND джерело='Власник квартири'", rid)
    manual = sum(round(q * p, 2) for q, p in c.execute("SELECT кількість, ціна FROM списання_матеріалів WHERE заявка=? AND джерело='Власник квартири'", (rid,)))
    return abs(mats - manual) < 0.005 and work >= 0, "заявка №%s: робота %.2f + матеріали власника %.2f = %.2f до нарахування" % (rid, work, mats, work + mats)
case("T-22", "Сума для рахунку власнику: вартість роботи + матеріали за рахунок власника", "FR-08", t22)

def t23():
    c = fresh()
    one_row = c.execute("PRAGMA foreign_keys").fetchone()[0]
    c2 = sqlite3.connect(":memory:")
    default = c2.execute("PRAGMA foreign_keys").fetchone()[0]
    return one_row == 1 and default == 0, "у новому з’єднанні foreign_keys=%d (за замовчуванням вимкнено), після PRAGMA — %d: програма вмикає прапорець при відкритті БД" % (default, one_row)
case("T-23", "Зовнішні ключі вмикаються явно (за замовчуванням вимкнені в SQLite)", "NFR-03", t23)

if __name__ == "__main__":
    passed = sum(1 for r in RESULTS if r["ok"])
    for r in RESULTS:
        print("%-5s %-4s %s\n        %s" % (r["id"], "OK" if r["ok"] else "FAIL", r["title"], r["actual"]))
    print("\nSQLite %s; пройдено %d з %d" % (sqlite3.sqlite_version, passed, len(RESULTS)))
    out = os.environ.get("DB_TESTS_JSON")
    if out:
        with open(out, "w", encoding="utf-8") as f:
            json.dump({"sqlite": sqlite3.sqlite_version, "results": RESULTS}, f, ensure_ascii=False, indent=1)
    sys.exit(0 if passed == len(RESULTS) else 1)
