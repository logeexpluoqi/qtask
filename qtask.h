/*
 * @Author: luoqi
 * @Date: 2021-04-29 19:27:49
 * @ Modified by: luoqi
 * @ Modified time: 2025-07-08 00:31
 */

#ifndef _QTASK_H
#define _QTASK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/* ==========================================================================
 * Configuration
 * ========================================================================== */

/**
 * @brief Enable per-task execution time measurement, recorded by qtask_exec().
 *
 * Costs 4 extra bytes per task and 4 per scheduler. The result is the time a
 * handler took, in QTASK_NOW() units.
 */
#ifndef QTASK_USE_EXEC_TIME
#define QTASK_USE_EXEC_TIME 0
#endif

/**
 * @brief Time source used when QTASK_USE_EXEC_TIME is enabled.
 *
 * Defaults to the scheduler tick counter, so the resolution is one tick.
 * A project may override it, e.g. with a free-running hardware counter, for
 * finer resolution; it must only count up and may wrap around.
 */
#ifndef QTASK_NOW
#define QTASK_NOW(core) ((core)->tick_cnt)
#endif

/**
 * @brief Optional critical section hooks around every list modification.
 *
 * Only needed when qtask_tick_increase() runs in an interrupt while tasks are
 * added or removed in thread context. Both default to no-ops, so the library
 * stays platform independent; a project may define them at build time.
 *
 *   QTASK_LOCK()      save and disable interrupts, returns the saved state
 *   QTASK_UNLOCK(key) restore the state returned by QTASK_LOCK()
 */
#ifndef QTASK_LOCK
#define QTASK_LOCK() ((uint32_t)0)
#endif

#ifndef QTASK_UNLOCK
#define QTASK_UNLOCK(key) ((void)(key))
#endif

/* ==========================================================================
 * Types
 * ========================================================================== */

/** @brief Node of the scheduler's doubly linked lists. */
typedef struct _task_list {
    struct _task_list *prev; /**< Previous node. */
    struct _task_list *next; /**< Next node. */
} QTaskList;

/** @brief Task entry point. */
typedef void (*QTaskHandle)(void);

/** @brief One task. The storage is owned by the caller (static or pool). */
typedef struct _qtask {
    const char *name;    /**< Unique name inside one scheduler. */
    uint8_t ready;       /**< Set when expired, cleared before the handler runs. */
    QTaskHandle handle;  /**< Executed when the countdown reaches zero. */
    size_t timer;        /**< Countdown in ticks until the next run. */
    size_t period;       /**< Countdown reload value, never 0. */
#if QTASK_USE_EXEC_TIME
    size_t exec_time;    /**< Duration of the last run, in QTASK_NOW() units. */
#endif
    QTaskList task_node; /**< Node in the scheduled or suspended list. */
} QTaskObj;

/** @brief Scheduler core. It owns the two lists, not the task objects. */
typedef struct {
    QTaskObj *run_task;     /**< Task inside its handler, NULL otherwise. */
    QTaskList task_list;    /**< Scheduled tasks. */
    QTaskList suspend_list; /**< Suspended tasks. */
#if QTASK_USE_EXEC_TIME
    size_t tick_cnt;        /**< Tick counter, default time base of exec_time. */
#endif
} QTaskCore;

/* ==========================================================================
 * API
 * ========================================================================== */

/**
 * @brief  Initializes the scheduler and both task lists.
 * @param  core  Scheduler core.
 */
void qtask_init(QTaskCore *core);

/**
 * @brief  Adds a task, or re-configures the same object if it is suspended.
 * @param  core    Scheduler core.
 * @param  task    Task storage, owned by the caller.
 * @param  name    Unique name inside the core.
 * @param  handle  Function executed when the task becomes ready.
 * @param  tick    Period in ticks, 0 is treated as 1.
 * @return 0 on success, 1 if this object is already scheduled (left untouched),
 *         -1 on invalid arguments or when the name belongs to another task.
 */
int qtask_add(QTaskCore *core, QTaskObj *task, const char *name, QTaskHandle handle, size_t tick);

/**
 * @brief  Unschedules a task by moving it to the suspended list.
 * @param  core  Scheduler core.
 * @param  task  Task to unschedule.
 * @return 0 on success, -1 if the task is not scheduled.
 */
int qtask_del(QTaskCore *core, QTaskObj *task);

/**
 * @brief  Unlinks a task from both lists so its storage may be freed.
 *         The object itself is not freed.
 * @param  core  Scheduler core.
 * @param  task  Task to unlink, scheduled or suspended.
 * @return 0 on success, -1 if the task was not linked.
 */
int qtask_remove(QTaskCore *core, QTaskObj *task);

/**
 * @brief  Suspends a scheduled task by name.
 * @param  core  Scheduler core.
 * @param  name  Task name.
 * @return 0 on success, -1 if no scheduled task has that name.
 */
int qtask_suspend(QTaskCore *core, const char *name);

/**
 * @brief  Resumes a suspended task by name, restarting its countdown.
 * @param  core  Scheduler core.
 * @param  name  Task name.
 * @return 0 on success, -1 if no suspended task has that name.
 */
int qtask_resume(QTaskCore *core, const char *name);

/**
 * @brief  Runs every ready task once and records its execution time.
 *
 * Handlers may add, delete, suspend or resume tasks: the list is re-scanned
 * whenever a node that was not visited yet gets unlinked.
 *
 * @param  core  Scheduler core.
 */
void qtask_exec(QTaskCore *core);

/**
 * @brief  Finds a task by name: scheduled list first, then suspended list.
 * @param  core      Scheduler core.
 * @param  taskname  Task name.
 * @return The task, or NULL if no task has that name.
 */
QTaskObj *qtask_obj(QTaskCore *core, const char *taskname);

/**
 * @brief  Advances every countdown by one tick and marks expired tasks ready.
 * @note   Call from a periodic interrupt, e.g. SysTick.
 * @param  core  Scheduler core.
 */
void qtask_tick_increase(QTaskCore *core);

/**
 * @brief  Reloads the countdown of the task currently executing, once.
 * @note   Must be called from inside a task handler; the period is unchanged.
 * @param  core  Scheduler core.
 * @param  tick  New countdown, 0 is treated as 1.
 */
void qtask_sleep(QTaskCore *core, size_t tick);

/**
 * @brief  Changes a task period, effective from its next countdown reload.
 * @param  task  Task.
 * @param  tick  New period, 0 is treated as 1.
 */
void qtask_tick_set(QTaskObj *task, size_t tick);

#ifdef __cplusplus
}
#endif

#endif
