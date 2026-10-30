/*
 * @Author: luoqi
 * @Date: 2021-04-29 19:27:09
 * @ Modified by: luoqi
 * @ Modified time: 2025-07-08 00:31
 */

#include <stddef.h>
#include "qtask.h"

/* Iterate every node of a list; the head node acts as the sentinel. */
#define QTASK_ITERATOR(node, list) for(node = (list)->next; node != (list); node = node->next)

/* Address of the structure that contains the given list node. */
#define QTASK_ENTRY(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))

static inline void list_insert_(QTaskList *list, QTaskList *node)
{
    list->next->prev = node;
    node->next = list->next;
    list->next = node;
    node->prev = list;
}

static inline void list_remove_(QTaskList *node)
{
    node->next->prev = node->prev;
    node->prev->next = node->next;
    node->next = node->prev = node;
}

/* Node identity test, safe to call with a node that was never inserted. */
static inline int list_contains_(QTaskList *list, QTaskList *node)
{
    QTaskList *cur;

    for(cur = list->next; cur != list; cur = cur->next) {
        if(cur == node) {
            return 1;
        }
    }
    return 0;
}

/* Name identity test without pulling in strcmp. */
static int name_eq_(const char *a, const char *b)
{
    while((*a == *b) && (*a != '\0')) {
        a++;
        b++;
    }
    return *a == *b;
}

/* Look up a task by its name. */
static QTaskObj *list_find_(QTaskList *list, const char *name)
{
    QTaskList *node;
    QTaskObj *task;

    QTASK_ITERATOR(node, list)
    {
        task = QTASK_ENTRY(node, QTaskObj, task_node);
        if(name_eq_(task->name, name)) {
            return task;
        }
    }
    return NULL;
}

void qtask_init(QTaskCore *core)
{
    if(!core) {
        return;
    }
    core->run_task = NULL;
    core->task_list.prev = core->task_list.next = &core->task_list;
    core->suspend_list.prev = core->suspend_list.next = &core->suspend_list;
#if QTASK_USE_EXEC_TIME
    core->tick_cnt = 0;
#endif
}

int qtask_add(QTaskCore *core, QTaskObj *task, const char *name, QTaskHandle handle, size_t tick)
{
    QTaskObj *other;
    uint32_t key;
    int ret = 0;

    if(!core || !task || !name || !handle) {
        return -1;
    }

    key = QTASK_LOCK();

    if(list_contains_(&core->task_list, &task->task_node)) {
        ret = 1; /* this very task object is already scheduled, keep it as is */
        goto out;
    }

    /* a name is owned by at most one task object, never replace silently */
    other = list_find_(&core->task_list, name);
    if(other == NULL) {
        other = list_find_(&core->suspend_list, name);
    }
    if((other != NULL) && (other != task)) {
        ret = -1;
        goto out;
    }

    if(list_contains_(&core->suspend_list, &task->task_node)) {
        list_remove_(&task->task_node); /* re-registering a suspended task object */
    }

    task->name = name;
    task->ready = 0;
    task->handle = handle;
    task->timer = tick ? tick : 1;
    task->period = tick ? tick : 1;
#if QTASK_USE_EXEC_TIME
    task->exec_time = 0;
#endif

    list_insert_(&core->task_list, &task->task_node);
out:
    QTASK_UNLOCK(key);
    return ret;
}

int qtask_del(QTaskCore *core, QTaskObj *task)
{
    uint32_t key;

    if(!core || !task) {
        return -1;
    }

    key = QTASK_LOCK();
    if(!list_contains_(&core->task_list, &task->task_node)) {
        QTASK_UNLOCK(key);
        return -1;
    }

    task->ready = 0;
    task->timer = task->period;
    list_remove_(&task->task_node);
    list_insert_(&core->suspend_list, &task->task_node);
    QTASK_UNLOCK(key);
    return 0;
}

int qtask_remove(QTaskCore *core, QTaskObj *task)
{
    uint32_t key;
    int ret = -1;

    if(!core || !task) {
        return -1;
    }

    key = QTASK_LOCK();
    if(list_contains_(&core->task_list, &task->task_node) ||
       list_contains_(&core->suspend_list, &task->task_node)) {
        task->ready = 0;
        list_remove_(&task->task_node);
        ret = 0;
    }
    QTASK_UNLOCK(key);
    return ret;
}

int qtask_suspend(QTaskCore *core, const char *name)
{
    QTaskObj *task;
    uint32_t key;
    int ret = -1;

    if(!core || !name) {
        return -1;
    }

    key = QTASK_LOCK();
    task = list_find_(&core->task_list, name);
    if(task) {
        task->ready = 0;
        task->timer = task->period;
        list_remove_(&task->task_node);
        list_insert_(&core->suspend_list, &task->task_node);
        ret = 0;
    }
    QTASK_UNLOCK(key);
    return ret;
}

int qtask_resume(QTaskCore *core, const char *name)
{
    QTaskObj *task;
    uint32_t key;
    int ret = -1;

    if(!core || !name) {
        return -1;
    }

    key = QTASK_LOCK();
    task = list_find_(&core->suspend_list, name);
    if(task) {
        task->ready = 0;
        task->timer = task->period;
        list_remove_(&task->task_node);
        list_insert_(&core->task_list, &task->task_node);
        ret = 0;
    }
    QTASK_UNLOCK(key);
    return ret;
}

QTaskObj *qtask_obj(QTaskCore *core, const char *taskname)
{
    QTaskObj *task;

    if(!core || !taskname) {
        return NULL;
    }

    task = list_find_(&core->task_list, taskname);
    if(task == NULL) {
        task = list_find_(&core->suspend_list, taskname);
    }
    return task;
}

void qtask_exec(QTaskCore *core)
{
    QTaskList *node;
    QTaskObj *task;
    QTaskObj *prev;
#if QTASK_USE_EXEC_TIME
    size_t start;
#endif

    if(!core) {
        return;
    }

    prev = core->run_task;
    node = core->task_list.next;
    while(node != &core->task_list) {
        /* A handler unlinked this node (next points to itself): restart the
         * scan, every task already executed is no longer ready. */
        if(node->next == node) {
            node = core->task_list.next;
            continue;
        }

        task = QTASK_ENTRY(node, QTaskObj, task_node);
        if(task->ready) {
#if QTASK_USE_EXEC_TIME
            start = QTASK_NOW(core);
#endif
            task->ready = 0; /* before the handler: it may unlink the task */
            core->run_task = task;
            task->handle();
#if QTASK_USE_EXEC_TIME
            task->exec_time = QTASK_NOW(core) - start;
#endif
        }
        node = node->next;
    }
    core->run_task = prev;
}

void qtask_tick_increase(QTaskCore *core)
{
    QTaskList *node;
    QTaskObj *task;

    if(!core) {
        return;
    }

#if QTASK_USE_EXEC_TIME
    core->tick_cnt++;
#endif

    QTASK_ITERATOR(node, &core->task_list)
    {
        task = QTASK_ENTRY(node, QTaskObj, task_node);
        if(--task->timer == 0) {
            task->ready = 1;
            task->timer = task->period;
        }
    }
}

void qtask_sleep(QTaskCore *core, size_t tick)
{
    if(!core || !core->run_task) {
        return;
    }
    core->run_task->timer = tick ? tick : 1;
}

void qtask_tick_set(QTaskObj *task, size_t tick)
{
    if(!task) {
        return;
    }
    task->period = tick ? tick : 1;
}
