# QTask

QTask 是一个面向裸机（无操作系统）环境的轻量级周期任务调度库。它不创建线程，也不负责抢占或并行执行任务：硬件定时中断推进任务计时，主循环调用调度器执行已经到期的任务处理函数。

## 设计与行为

- **两阶段调度**：`qtask_tick_increase()` 通常由周期中断调用，只更新计时器并标记到期任务；`qtask_exec()` 在主循环中调用，执行任务回调。回调不会在中断中运行。
- **协作式执行**：任务回调按顺序运行，回调返回后才会执行下一个任务。回调应尽量短；长时间运行会推迟其他任务。
- **固定周期倒计时**：每个任务有当前倒计时 `countdown` 和重载值 `reload`。倒计时归零时任务变为 ready，并按重载值重新装载计时器。
- **不补跑积压周期**：ready 是一个标志而非次数。如果任务尚未执行时又到达一个或多个周期，仍只执行一次；它不是实时操作系统，也不保证严格的执行时刻。
- **调用方管理内存**：任务对象不由库分配或释放。对象必须在注册期间保持有效；同一个对象不能同时注册到多个调度器。任务名由库保存为指针而不复制，因此名称字符串也必须保持有效，并且在同一调度器中唯一。
- **可暂停与恢复**：调度任务和暂停任务分别保存在两个侵入式双向链表中。暂停或恢复会重新开始完整的周期倒计时。

## 文件构成

- `qtask.h`：配置宏、任务与调度器类型，以及全部公开 API；使用 `qtask_suspend()` 暂停任务，使用 `qtask_remove()` 将任务摘除。
- `qtask.c`：双向链表、任务注册与管理、tick 计时和回调执行逻辑。

将 `qtask.c` 编译进项目，并在需要使用 API 的源文件中包含 `qtask.h` 即可。库只依赖标准 C 头文件，不要求操作系统或动态内存分配。

## 快速开始

```c
#include "qtask.h"

static QTaskCore scheduler;
static QTaskObj heartbeat_task;
static QTaskObj sample_task;

static void heartbeat(void)
{
	/* 更新心跳指示 */
}

static void sample_sensor(void)
{
	/* 读取传感器 */
}

void app_init(void)
{
	qtask_init(&scheduler);
	qtask_add(&scheduler, &heartbeat_task, "heartbeat", heartbeat, 100);
	qtask_add(&scheduler, &sample_task, "sensor", sample_sensor, 10);
}

/* 在平台的固定周期定时中断中调用；这里的 tick 是调度器时间单位。 */
void systick_handler(void)
{
	qtask_tick_increase(&scheduler);
}

/* 在主循环中持续调用。 */
void app_loop(void)
{
	for (;;) {
		qtask_exec(&scheduler);
	}
}
```

上例中，`heartbeat` 每 100 个 tick 到期一次，`sample_sensor` 每 10 个 tick 到期一次。实际时间间隔取决于平台调用 `qtask_tick_increase()` 的频率。例如每 1 ms 调用一次时，100 tick 对应约 100 ms。中断处理函数只应调用 tick API，不要在那里直接执行任务回调。

## API

### 初始化与注册

```c
void qtask_init(QTaskCore *core);
int qtask_add(QTaskCore *core, QTaskObj *task,
			  const char *name, QTaskHandle handle, size_t tick);
```

- `qtask_init()` 初始化调度器及其任务链表。应在注册任务前调用。
- `qtask_add()` 注册任务，`tick` 是周期，单位与 tick 中断一致。`tick == 0` 会按 1 处理。
- 返回 `0` 表示成功；返回 `1` 表示同一个对象已经处于调度列表，原配置保持不变；返回 `-1` 表示参数无效，或任务名已被另一个对象使用。
- 已暂停的同一对象可以再次传给 `qtask_add()` 以重新配置并启用它。名称冲突不会静默替换已有任务。同一任务对象不能同时属于多个调度器。

任务回调类型为 `void (*)(void)`，不接收参数。需要上下文数据时，可在回调中访问应用状态，或通过 `qtask_obj()` 查找任务对象及其应用侧关联状态。

### 执行与计时

```c
void qtask_tick_increase(QTaskCore *core);
void qtask_exec(QTaskCore *core);
void qtask_sleep(QTaskCore *core, size_t tick);
void qtask_tick_set(QTaskObj *task, size_t tick);
```

- `qtask_tick_increase()` 将每个已调度任务的倒计时减一；到期后设置 ready 并重载周期。通常从固定周期定时中断调用。
- `qtask_exec()` 执行所有 ready 任务，每个任务在本次执行中至多调用一次。它会在调用回调前清除 ready，因此回调中增删、暂停或恢复任务是受支持的；被摘除的当前任务对象必须至少保留到回调返回以及当前 `qtask_exec()` 对该回调的处理完成。
- `qtask_sleep()` 只能在任务回调中调用。它将当前正在运行任务的下一次倒计时设为指定 tick 数，但不改变周期；`tick == 0` 按 1 处理。随后倒计时到期时，任务仍按原周期继续运行。
- `qtask_tick_set()` 修改任务的周期，作用于下一次倒计时重载；当前正在倒计时的值不会立即改变。`tick == 0` 按 1 处理。

### 暂停、恢复与查询

```c
int qtask_remove(QTaskCore *core, QTaskObj *task);
int qtask_suspend(QTaskCore *core, const char *name);
int qtask_resume(QTaskCore *core, const char *name);
QTaskObj *qtask_obj(QTaskCore *core, const char *taskname);
```

- `qtask_suspend()` 按名称暂停任务；`qtask_resume()` 按名称恢复任务，并从完整周期重新倒计时。两者成功返回 `0`，找不到对应任务时返回 `-1`。
- `qtask_remove()` 将任务从调度列表或暂停列表中彻底摘除。返回 `0` 表示成功，`-1` 表示该对象未链接到此调度器。该函数不释放对象内存。
- `qtask_obj()` 按名称查找任务，先查调度列表再查暂停列表；找不到或参数无效时返回 `NULL`。

如果在任务回调中摘除当前任务，不要在该回调返回前释放任务对象；应等当前 `qtask_exec()` 调用结束后再回收其存储。其他场景下，也应在 `qtask_remove()` 成功后再释放对象。

## 可选配置

在包含 `qtask.h` 前或编译时定义以下宏；相关配置必须对 `qtask.c` 和使用该头文件的代码保持一致。

- `QTASK_EXEC_TIME_ENABLE`：设为 `1` 后，`qtask_exec()` 会记录每个任务最近一次回调耗时，并在 `QTaskCore` 中启用 tick 计数器。耗时保存在任务对象的 `exec_time` 字段中，单位由 `QTASK_NOW()` 决定；默认使用 tick 计数器。每个任务和调度器各增加一个 `size_t` 字段。
- `QTASK_NOW(core)`：启用耗时统计时使用的递增时间源，默认返回 `(core)->tick_count`。可覆盖为平台的高分辨率计数器；计数器应递增且允许回绕。
- `QTASK_LOCK()` 与 `QTASK_UNLOCK(key)`：可覆盖为平台临界区钩子。若 tick 更新运行在中断中，而其他上下文会同时增删任务，应实现它们以在链表修改期间保存并屏蔽中断、随后恢复中断状态。默认实现为空操作，适用于无此类并发的场景。

`QTASK_EXEC_TIME_ENABLE` 必须对编译 `qtask.c` 和包含 `qtask.h` 的应用代码保持一致。耗时字段仅在 `QTASK_EXEC_TIME_ENABLE == 1` 时存在，因此应用代码不应在未启用该宏时访问 `exec_time`。默认临界区钩子为空操作；它们只保护增删、暂停和恢复任务时的链表修改，不会自动使所有 API 都线程安全。
