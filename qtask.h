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

#ifndef QNULL
#define QNULL ((void *)0)
#endif

/**
 * @struct QTaskList
 * @brief Represents a node in a doubly linked list.
 * 
 * This structure is used to implement a doubly linked list for task scheduling.
 */
typedef struct _task_list {
    struct _task_list* prev; /**< Pointer to the previous node in the list. */
    struct _task_list* next; /**< Pointer to the next node in the list. */
} QTaskList;

/**
 * @struct QTaskObj
 * @brief Represents a task object.
 * 
 * This structure encapsulates all the information related to a task, including its name, ID,
 * execution function, timing information, and usage description.
 */
typedef struct _qtask
{
    const char* name;       /**< Name of the task. */
    uint16_t id;            /**< Unique identifier of the task */
    uint8_t isready;        /**< Flag indicating whether the task is ready to execute. */
    void (*handle)(void); /**< Function pointer to the task's execution function. */
    size_t timer;         /**< Timer value for the task, counting down to execution. */
    size_t period;          /**< Periodic tick value for the task. */
    size_t rtime;         /**< Recorded execution time of the task. */
    size_t rtick;         /**< Running tick count of the task. */
    QTaskList task_node;    /**< Doubly linked list node for task scheduling. */
} QTaskObj;

/**
 * @typedef QTaskHandle
 * @brief Function pointer type for task execution functions.
 * 
 * Task execution functions should accept a void pointer as an argument.
 */
typedef void (*QTaskHandle)(void);

/**
 * @struct QTaskSched
 * @brief Represents a task scheduler.
 * 
 * This structure manages the scheduling of tasks, including task arguments, target task ID,
 * the currently running task, and two doubly linked lists for scheduled and unscheduled tasks.
 */
typedef struct
{
    void *args;             /**< Arguments to be passed to the task. */
    QTaskObj *run_task;     /**< Pointer to the currently running task. */
    QTaskList task_list;   /**< Doubly linked list for scheduled tasks. */
    QTaskList suspend_list; /**< Doubly linked list for unscheduled tasks. */
} QTaskSched;

/**
 * @brief Initializes the task scheduler.
 * 
 * This function initializes the scheduled and unscheduled task lists of the task scheduler.
 * 
 * @param sched Pointer to the task scheduler object.
 */
void qtask_sched_init(QTaskSched *sched);

/**
 * @brief Adds a task to the task scheduler.
 * 
 * This function initializes a task object and adds it to the scheduled task list if it's not already there.
 * If the task exists in the unscheduled list, it will be removed first.
 * 
 * @param sched Pointer to the task scheduler object.
 * @param task Pointer to the task object to be added.
 * @param name Name of the task.
 * @param handle Function pointer to the task's execution function.
 * @param tick Periodic tick value for the task.
 * @return 0 if the task is successfully added, 1 if the task already exists in the scheduled list.
 */
int qtask_add(QTaskSched *sched, QTaskObj* task, const char* name, QTaskHandle handle, size_t tick);

/**
 * @brief Removes a task from the task scheduler.
 * 
 * This function removes the task from the scheduled list if it exists and adds it to the unscheduled list.
 * 
 * @param sched Pointer to the task scheduler object.
 * @param task Pointer to the task object to be removed.
 * @return 0 if the task is successfully removed and added to the unscheduled list, -1 otherwise.
 */
int qtask_del(QTaskSched *sched, QTaskObj* task);

/**
 * @brief Schedules a task for execution.
 * 
 * This function adds the task to the scheduled list if it is not already scheduled.
 * 
 * @param sched Pointer to the task scheduler object.
 * @param task Pointer to the task object to be scheduled.
 * @return 0 if the task is successfully added to the scheduled list, -1 otherwise.
 */
int qtask_suspend(QTaskSched *sched, const char *name);

/**
 * @brief Resumes a suspended task.
 * 
 * This function resumes the specified task by removing it from the suspended list.
 * 
 * @param sched Pointer to the task scheduler object.
 * @param task Pointer to the task object to be resumed.
 * @return 0 if the task is successfully resumed, -1 otherwise.
 */
int qtask_resume(QTaskSched *sched, const char *name);

/**
 * @brief Executes all ready tasks in the task scheduler.
 * 
 * This function iterates through the scheduled task list and executes all tasks marked as ready.
 * 
 * @param sched Pointer to the task scheduler object.
 */
void qtask_exec(QTaskSched *sched);

/**
 * @brief Retrieves a task object by its name.
 * 
 * This function searches the scheduled task list for a task with the specified name.
 * 
 * @param sched Pointer to the task scheduler object.
 * @param taskname Name of the task to retrieve.
 * @return Pointer to the task object if found, QTASK_NONE otherwise.
 */
QTaskObj *qtask_obj(QTaskSched *sched, const char *taskname);

/**
 * @brief Increases the timer count of all tasks in the task scheduler.
 * 
 * This function should be called periodically to update the timer values of all tasks.
 * When a task's timer reaches zero, it will be marked as ready for execution.
 * 
 * @param sched Pointer to the task scheduler object.
 */
void qtask_tick_increase(QTaskSched *sched);

/**
 * @brief Measures the execution time of tasks.
 * 
 * This function should be called in a timer interrupt function with a higher frequency than
 * qtask_tick_increase to measure the execution time of task callback functions.
 * 
 * @param sched Pointer to the task scheduler object.
 */
void qtask_runtime_increase(QTaskSched *sched);

/**
 * @brief Changes the periodic time of a running task.
 * 
 * This function changes the timer value of the currently running task.
 * 
 * @param sched Pointer to the task scheduler object.
 * @param tick New periodic tick value for the task.
 */
void qtask_sleep(QTaskSched *sched, size_t tick);

/**
 * @brief Changes the periodic time of a task.
 * 
 * This function changes the periodic tick value of a specified task.
 * 
 * @param task Pointer to the task object.
 * @param tick New periodic tick value for the task.
 */
void qtask_tick_set(QTaskObj *task, size_t tick);

#ifdef __cplusplus
 }
#endif

#endif
