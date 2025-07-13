/*
 * @Author: luoqi
 * @Date: 2021-04-29 19:27:09
 * @ Modified by: luoqi
 * @ Modified time: 2025-07-08 00:31
 */

#include <stddef.h>
#include "qtask.h"

 // Macro for iterating through a doubly linked list
#define QTASK_ITERATOR(node, list)  \
    for (node = (list)->next; node != (list); node = node->next)

#define QTASK_ITERATOR_SAFE(node, safe, list) \
    for (node = (list) ? (list)->next : QNULL, safe = node ? node->next : QNULL; \
         node && node != (list); \
         node = safe, safe = node ? node->next : QNULL)

// Macro for getting the pointer to the structure containing the doubly linked list node
#define QTASK_ENTRY(ptr, type, member)  \
   ((type *)((char *)(ptr) - ((size_t) &((type*)0)->member)))

static inline void _list_insert(QTaskList *list, QTaskList *node)
{
    list->next->prev = node;
    node->next = list->next;
    list->next = node;
    node->prev = list;
}

static inline void _list_remove(QTaskList *node)
{
    node->next->prev = node->prev;
    node->prev->next = node->next;
    node->next = node->prev = node;
}

static int _qtask_isexist(QTaskSched *sched, QTaskObj *task)
{
    QTaskList *node;
    QTaskObj *_task;

    QTASK_ITERATOR(node, &sched->task_list)
    {
        _task = QTASK_ENTRY(node, QTaskObj, task_node);
        if(task->id == _task->id) {
            return 1;
        }
    }
    return 0;
}

static int _qdtask_isexsit(QTaskSched *sched, QTaskObj *task)
{
    QTaskList *node;
    QTaskObj *_task;

    QTASK_ITERATOR(node, &sched->suspend_list)
    {
        _task = QTASK_ENTRY(node, QTaskObj, task_node);
        if(task->id == _task->id) {
            return 1;
        }
    }
    return 0;
}

static uint16_t _id_calc(const char *name)
{
    if(!name) {
        return 0;
    }
    uint16_t hash = 5381;
    int c;
    while((c = *name++)) {
        hash = (hash << 5) + hash + c; // hash * 33 + c
    }
    return hash;
}

void qtask_sched_init(QTaskSched *sched)
{
    sched->task_list.prev = sched->task_list.next = &sched->task_list;
    sched->suspend_list.prev = sched->suspend_list.next = &sched->suspend_list;
}

int qtask_add(QTaskSched *sched, QTaskObj *task, const char *name, QTaskHandle handle, size_t tick)
{
    task->name = name;
    task->id = _id_calc(name);
    task->isready = 0;
    task->handle = handle;
    task->timer = tick;
    task->period = tick;
    task->rtime = 0;
    task->rtick = 0;

    if(_qdtask_isexsit(sched, task)) {
        task->isready = 0;
        task->timer = task->period;
        _list_remove(&task->task_node);
    }

    if(!_qtask_isexist(sched, task)) {
        _list_insert(&sched->task_list, &task->task_node);
        return 0;
    }
    return 1;
}

int qtask_del(QTaskSched *sched, QTaskObj *task)
{
    if(_qtask_isexist(sched, task)) {
        task->isready = 0;
        task->timer = task->period;
        _list_remove(&task->task_node);
    }

    if(!_qdtask_isexsit(sched, task)) {
        _list_insert(&sched->suspend_list, &task->task_node);
        return 0;
    }
    return -1;
}

int qtask_suspend(QTaskSched *sched, const char *name)
{
    QTaskList *node, *safe;
    QTaskObj *task;
    uint16_t id = _id_calc(name);

    QTASK_ITERATOR_SAFE(node, safe, &sched->task_list)
    {
        task = QTASK_ENTRY(node, QTaskObj, task_node);
        if(task->id == id) {
            if(_qtask_isexist(sched, task)) {
                task->isready = 0;
                task->timer = task->period;
                _list_remove(&task->task_node);
            }

            if(!_qdtask_isexsit(sched, task)) {
                _list_insert(&sched->suspend_list, &task->task_node);
                return 0;
            }
            return -1;
        }
    }
    return -1;

}

int qtask_resume(QTaskSched *sched, const char *name)
{
    QTaskList *node, *safe;
    QTaskObj *task;
    uint16_t id = _id_calc(name);

    QTASK_ITERATOR_SAFE(node, safe, &sched->suspend_list)
    {
        task = QTASK_ENTRY(node, QTaskObj, task_node);
        if(task->id == id) {
            if(_qdtask_isexsit(sched, task)) {
                task->isready = 0;
                task->timer = task->period;
                _list_remove(&task->task_node);
            }

            if(!_qtask_isexist(sched, task)) {
                _list_insert(&sched->task_list, &task->task_node);
                return 0;
            }
        }
    }
    return -1;
}

QTaskObj *qtask_obj(QTaskSched *sched, const char *taskname)
{
    QTaskList *node;
    QTaskObj *task;
    uint16_t id = _id_calc(taskname);

    QTASK_ITERATOR(node, &sched->task_list)
    {
        task = QTASK_ENTRY(node, QTaskObj, task_node);
        if(task->id == id) {
            return task;
        }
    }
    return QNULL;
}

void qtask_exec(QTaskSched *sched)
{
    QTaskList *node, *safe;
    QTaskObj *task;

    QTASK_ITERATOR_SAFE(node, safe, &sched->task_list)
    {
        task = QTASK_ENTRY(node, QTaskObj, task_node);
        if(task->isready) {
            task->handle();
            task->rtime = task->rtick;
            task->isready = 0;
            task->rtick = 0;
        }
    }
}

void qtask_tick_increase(QTaskSched *sched)
{
    QTaskList *node, *safe;
    QTaskObj *task;
    int count = 0;

    QTASK_ITERATOR_SAFE(node, safe, &sched->task_list)
    {
        if(node == QNULL || node->next == QNULL || node->prev == QNULL) {
            return;
        }

        task = QTASK_ENTRY(node, QTaskObj, task_node);
        if(task == QNULL) {
            return;
        }

        if(task->timer > 0) {
            if(--task->timer <= 0) {
                task->isready = 1;
                sched->run_task = task;
                task->timer = task->period;
            }
        }
        count++;

        if(count > 1000) {
            return;
        }
    }
}

void qtask_runtime_increase(QTaskSched *sched)
{
    QTaskList *node;
    QTaskObj *task;

    QTASK_ITERATOR(node, &sched->task_list)
    {
        task = QTASK_ENTRY(node, QTaskObj, task_node);
        if(task->isready) {
            task->rtick++;
        }
    }
}

void qtask_sleep(QTaskSched *sched, size_t tick)
{
    if(!sched->run_task) {
        sched->run_task->timer = tick;
    }
}

void qtask_tick_set(QTaskObj *obj, size_t tick)
{
    obj->period = tick;
}
