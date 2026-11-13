/*
 * @Author: luoqi
 * @Date: 2021-04-29 19:27:49
 * @ Modified by: luoqi
 * @ Modified time: 2025-07-08 00:31
 */

#ifndef _QTASK_H_
#define _QTASK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/* Measures handler execution time in QTASK_NOW() units.
 * Costs one size_t per task and one per scheduler. */
#ifndef QTASK_EXEC_TIME_ENABLE
#define QTASK_EXEC_TIME_ENABLE 0
#endif

#if QTASK_EXEC_TIME_ENABLE

/* Time base of exec_time, defaults to the tick counter; override it for a finer
 * resolution. Must only count up, may wrap around. */
#ifndef QTASK_NOW
#define QTASK_NOW(core) ((core)->tick_count)
#endif

#endif /* QTASK_EXEC_TIME_ENABLE */

/* Critical section hooks around list modifications, only needed when
 * qtask_tick_increase() runs in an interrupt while tasks are added or removed
 * in thread context. Both default to no-ops.
 *   QTASK_LOCK()      disables interrupts, returns the saved state
 *   QTASK_UNLOCK(key) restores it */
#ifndef QTASK_LOCK
#define QTASK_LOCK() ((uint32_t)0)
#endif

#ifndef QTASK_UNLOCK
#define QTASK_UNLOCK(key) ((void)(key))
#endif

/** @brief Node of the scheduler's doubly linked lists. */
typedef struct QTaskList QTaskList;
struct QTaskList {
    struct QTaskList *prev;
    struct QTaskList *next;
};

/** @brief Task entry point. */
typedef void (*QTaskHandle)(void);

/** @brief One task; storage owned by the caller. */
typedef struct _qtask {
    const char *name;    /**< Unique name inside one scheduler. */
    uint8_t ready;       /**< Set when expired, cleared before the handler runs. */
    QTaskHandle handler; /**< Called when the countdown reaches zero. */
    size_t countdown;    /**< Ticks left until the next run. */
    size_t reload;       /**< Countdown reload value, never 0. */
#if QTASK_EXEC_TIME_ENABLE
    size_t exec_time;    /**< Duration of the last run, in QTASK_NOW() units. */
#endif
    QTaskList node;      /**< Node in the scheduled or suspended list. */
} QTaskObj;

/** @brief Scheduler core. Owns the two lists, not the task objects. */
typedef struct {
    QTaskObj *current_task; /**< Task inside its handler, NULL otherwise. */
    QTaskList sched_list;
    QTaskList suspend_list;
#if QTASK_EXEC_TIME_ENABLE
    size_t tick_count;      /**< Tick counter, default time base of exec_time. */
#endif
} QTaskCore;

/** @brief Initializes the scheduler and both task lists. */
void qtask_init(QTaskCore *core);

/**
 * @brief  Adds a task, or re-configures the same object if it is suspended.
 * @param  tick  Period in ticks, 0 is treated as 1.
 * @return 0 on success, 1 if this object is already scheduled, -1 on invalid
 *         arguments or when the name belongs to another task.
 */
int qtask_add(QTaskCore *core, QTaskObj *task, const char *name, QTaskHandle handle, size_t tick);

/**
 * @brief  Unlinks a task from both lists, without freeing it.
 * @return 0 on success, -1 if the task was not linked.
 */
int qtask_remove(QTaskCore *core, QTaskObj *task);

/**
 * @brief  Moves a scheduled task to the suspended list.
 * @return 0 on success, -1 if no scheduled task has that name.
 */
int qtask_suspend(QTaskCore *core, const char *name);

/**
 * @brief  Moves a suspended task back and restarts its countdown.
 * @return 0 on success, -1 if no suspended task has that name.
 */
int qtask_resume(QTaskCore *core, const char *name);

/**
 * @brief  Runs every ready task once and records its execution time.
 *
 * Handlers may add, remove, suspend or resume tasks: the list is re-scanned
 * when a not yet visited node gets unlinked.
 */
void qtask_exec(QTaskCore *core);

/** @brief Finds a task by name, scheduled list first, then suspended list;
 *         returns NULL if no task has that name. */
QTaskObj *qtask_obj(QTaskCore *core, const char *taskname);

/**
 * @brief  Advances every countdown by one tick and flags expired tasks ready.
 * @note   Call from a periodic interrupt, e.g. SysTick.
 */
void qtask_tick_increase(QTaskCore *core);

/**
 * @brief  Reloads the countdown of the running task once; reload is unchanged.
 * @param  tick  New countdown, 0 is treated as 1.
 * @note   Must be called from inside a task handler.
 */
void qtask_sleep(QTaskCore *core, size_t tick);

/**
 * @brief  Changes the reload value, effective from the next reload.
 * @param  tick  New reload value, 0 is treated as 1.
 */
void qtask_tick_set(QTaskObj *task, size_t tick);

#ifdef __cplusplus
}
#endif

#endif /* _QTASK_H_ */
