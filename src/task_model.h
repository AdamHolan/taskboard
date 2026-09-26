#ifndef TASK_MODEL_H
#define TASK_MODEL_H

#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#define TASK_TITLE_CAP 96
#define TASK_CATEGORY_CAP 48
#define TASK_MAX_COUNT 128

typedef struct Date {
    int year;
    int month;
    int day;
} Date;

typedef enum RepeatUnit {
    REPEAT_NONE,
    REPEAT_DAY,
    REPEAT_WEEK,
    REPEAT_MONTH,
    REPEAT_YEAR
} RepeatUnit;

typedef enum TaskKind {
    TASK_FINITE,
    TASK_RECURRING
} TaskKind;

typedef struct Task {
    uint32_t id;
    wchar_t title[TASK_TITLE_CAP];
    wchar_t category[TASK_CATEGORY_CAP];
    Date start;
    Date due;
    RepeatUnit repeat;
    unsigned repeat_interval;
    TaskKind kind;
    unsigned color_rgb;
    unsigned shape;
    int completed;
} Task;

typedef struct TaskStore {
    Task items[TASK_MAX_COUNT];
    size_t count;
    uint32_t next_id;
} TaskStore;

void task_store_init(TaskStore *store);
int task_store_add(TaskStore *store, const Task *task, wchar_t *error, size_t error_cap);
int task_occurs_on(const Task *task, Date date);
int date_is_valid(Date date);
int date_compare(Date a, Date b);
int days_in_month(int year, int month);
const wchar_t *repeat_name(RepeatUnit unit);

#endif

