#include "task_model.h"

#include <string.h>

static void set_error(wchar_t *out, size_t cap, const wchar_t *message)
{
    if (out && cap) {
        wcsncpy(out, message, cap - 1);
        out[cap - 1] = L'\0';
    }
}

int days_in_month(int year, int month)
{
    static const int lengths[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    int leap;
    if (month < 1 || month > 12) return 0;
    leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return lengths[month - 1] + (month == 2 && leap);
}

int date_is_valid(Date date)
{
    return date.year >= 1601 && date.year <= 9999 && date.month >= 1 &&
           date.month <= 12 && date.day >= 1 &&
           date.day <= days_in_month(date.year, date.month);
}

int date_compare(Date a, Date b)
{
    if (a.year != b.year) return a.year < b.year ? -1 : 1;
    if (a.month != b.month) return a.month < b.month ? -1 : 1;
    if (a.day != b.day) return a.day < b.day ? -1 : 1;
    return 0;
}

/* Howard Hinnant's civil-date conversion, adapted to C.  Only differences are
   used, so the choice of epoch is immaterial. */
static int64_t date_serial(Date date)
{
    int year = date.year - (date.month <= 2);
    int era = (year >= 0 ? year : year - 399) / 400;
    unsigned yoe = (unsigned)(year - era * 400);
    unsigned month_index = (unsigned)(date.month + (date.month > 2 ? -3 : 9));
    unsigned doy = (153 * month_index + 2) / 5 + (unsigned)date.day - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (int64_t)era * 146097 + (int64_t)doe;
}

const wchar_t *repeat_name(RepeatUnit unit)
{
    switch (unit) {
    case REPEAT_DAY: return L"Daily";
    case REPEAT_WEEK: return L"Weekly";
    case REPEAT_MONTH: return L"Monthly";
    case REPEAT_YEAR: return L"Yearly";
    default: return L"Does not repeat";
    }
}

int task_store_add(TaskStore *store, const Task *source, wchar_t *error, size_t error_cap)
{
    Task task;
    if (!store || !source) return 0;
    if (!source->title[0]) {
        set_error(error, error_cap, L"Enter a task name.");
        return 0;
    }
    if (!date_is_valid(source->start)) {
        set_error(error, error_cap, L"The start date is invalid.");
        return 0;
    }
    if (source->kind == TASK_FINITE && date_is_valid(source->due) &&
        date_compare(source->due, source->start) < 0) {
        set_error(error, error_cap, L"The due date cannot precede the start date.");
        return 0;
    }
    if (store->count == TASK_MAX_COUNT) {
        set_error(error, error_cap, L"The prototype task store is full.");
        return 0;
    }
    task = *source;
    task.id = store->next_id++;
    if (!task.category[0]) wcscpy(task.category, L"Uncategorized");
    if (task.repeat_interval == 0) task.repeat_interval = 1;
    store->items[store->count++] = task;
    set_error(error, error_cap, L"");
    return 1;
}

int task_occurs_on(const Task *task, Date date)
{
    int months;
    int64_t elapsed_days;
    if (!task || !date_is_valid(date) || date_compare(date, task->start) < 0) return 0;
    if (task->repeat == REPEAT_NONE) return date_compare(date, task->start) == 0;
    switch (task->repeat) {
    case REPEAT_DAY:
        elapsed_days = date_serial(date) - date_serial(task->start);
        return elapsed_days % task->repeat_interval == 0;
    case REPEAT_WEEK:
        elapsed_days = date_serial(date) - date_serial(task->start);
        return elapsed_days % (7 * task->repeat_interval) == 0;
    case REPEAT_MONTH:
        months = (date.year - task->start.year) * 12 + date.month - task->start.month;
        return months >= 0 && months % (int)task->repeat_interval == 0 &&
               date.day == (task->start.day <= days_in_month(date.year, date.month)
                            ? task->start.day : days_in_month(date.year, date.month));
    case REPEAT_YEAR:
        return date.month == task->start.month && date.day == task->start.day &&
               (date.year - task->start.year) % (int)task->repeat_interval == 0;
    default:
        return 0;
    }
}

void task_store_init(TaskStore *store)
{
    Task sample;
    wchar_t ignored[2];
    memset(store, 0, sizeof(*store));
    store->next_id = 1;

    memset(&sample, 0, sizeof(sample));
    wcscpy(sample.title, L"Clean bathroom");
    wcscpy(sample.category, L"Household maintenance");
    sample.start = (Date){ 2026, 9, 12 };
    sample.repeat = REPEAT_MONTH;
    sample.repeat_interval = 1;
    sample.kind = TASK_RECURRING;
    sample.color_rgb = 0x4B8D6A;
    sample.shape = 0;
    task_store_add(store, &sample, ignored, 2);

    memset(&sample, 0, sizeof(sample));
    wcscpy(sample.title, L"Renew car registration");
    wcscpy(sample.category, L"Administration");
    sample.start = (Date){ 2026, 9, 21 };
    sample.due = (Date){ 2026, 9, 28 };
    sample.repeat = REPEAT_NONE;
    sample.kind = TASK_FINITE;
    sample.color_rgb = 0x5672B8;
    sample.shape = 1;
    task_store_add(store, &sample, ignored, 2);

    memset(&sample, 0, sizeof(sample));
    wcscpy(sample.title, L"Dentist appointment");
    wcscpy(sample.category, L"Health");
    sample.start = (Date){ 2026, 9, 25 };
    sample.repeat = REPEAT_YEAR;
    sample.repeat_interval = 1;
    sample.kind = TASK_RECURRING;
    sample.color_rgb = 0xA05C72;
    sample.shape = 2;
    task_store_add(store, &sample, ignored, 2);
}
