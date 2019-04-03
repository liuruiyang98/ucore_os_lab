#include <defs.h>
#include <list.h>
#include <proc.h>
#include <assert.h>
#include <cfs_sched.h>

/************************** cfs sched *******************************/
/* LAB6 challenge: 2016011396*/
#define USE_SKEW_HEAP 1

#define NICE_0_LOAD 1024            // 所有进程都以 nice 为 0 的进程的权重 1024 作为基准，计算自己的 vruntime 增加速度。
#define BIG_STRIDE  0x7FFFFFFF      /* you should give a value, and is ??? */

/* The compare function for two skew_heap_node_t's and the
 * corresponding procs*/
static int
proc_cfs_comp_f(void *a, void *b)
{
     struct proc_struct *p = le2proc(a, lab6_run_pool);
     struct proc_struct *q = le2proc(b, lab6_run_pool);
     int32_t c = p->lab6_stride - q->lab6_stride;
     if (c > 0) return 1;
     else if (c == 0) return 0;
     else return -1;
}

static void
cfs_init(struct run_queue *rq) {
    // 同 stride sched
    list_init(&(rq->run_list));         // 运行队列的哨兵结构，可以看作是队列头和尾，初始化队列中仅有哨兵自己
    rq->lab6_run_pool = NULL;           // 优先队列形式的进程容器初始化为空
    rq->proc_num = 0;                   // 当前就绪队列中的进程数初始化为 0 
    rq->proc_priotrity_tot = 0;         // 当前就绪队列中的进程数优先级之和初始化为 0
}

static void
cfs_enqueue(struct run_queue *rq, struct proc_struct *proc) {
    // 同 stride sched
    rq->lab6_run_pool = skew_heap_insert(rq->lab6_run_pool, &(proc->lab6_run_pool), proc_cfs_comp_f);       // 将进程插入到基于斜堆的优先队列中
    rq->proc_priotrity_tot += proc->lab6_priority;                                                     // 所有进程权重和增加
    proc->rq = rq;                          // 同 RR 算法，标记进程属于当前就绪队列
    rq->proc_num = rq->proc_num + 1;        // 同 RR 算法，就绪队列中的进程数量加1
}

static void
cfs_dequeue(struct run_queue *rq, struct proc_struct *proc) {
    // 同 stride sched
    assert(proc->rq == rq); 
    rq->lab6_run_pool = skew_heap_remove(rq->lab6_run_pool, &(proc->lab6_run_pool), proc_cfs_comp_f);    // 将进程插入从基于斜堆的优先队列中移除
    rq->proc_num = rq->proc_num - 1;                                                                     // 同 RR 算法，就绪队列中的进程数量减1
    rq->proc_priotrity_tot -= proc->lab6_priority;                                                  // 所有进程权重和衰减
}

static void
cfs_proc_tick(struct run_queue *rq, struct proc_struct *proc) {
    assert(proc != NULL);                       // 确保当前进程一定存在
    if (proc->time_slice > 0) {                 // 同 RR 算法，每一次时钟中断代表时间流失一小段，运行进程的时间片减一
        proc->time_slice --;
        proc->real_run_time ++;                 // 实际运行时间增加
    }
    if (proc->time_slice == 0) {                // 同 RR 算法，当运行进程耗尽时间片之后，则需要调度
        proc->need_resched = 1;
    }
}

// CFS sched 算法
static struct proc_struct *
cfs_pick_next(struct run_queue *rq) {
    // 同 stride sched
    if (rq->lab6_run_pool == NULL) return NULL;                                     // 如果就绪队列为空，则直接返回空

	struct proc_struct* todo_proc = le2proc(rq->lab6_run_pool, lab6_run_pool);      // 从就绪队列头部获取到对应调度的最优进程
    assert(todo_proc != NULL);                                                      // 就绪队列不为空，所以进程必定存在
    assert(todo_proc->lab6_priority >= 0);                                          // 优先级不可能小于 0
    // vruntime = 实际运行时间 * 1024 / 进程权重 = 调度周期 * 1024 / 所有进程总权重。
	if (todo_proc->lab6_priority == 0) {                                            // 更新对应进程的 vruntime 值，如果为 0 则设置为 NICE_0_LOAD
		todo_proc->lab6_stride += todo_proc->real_run_time * NICE_0_LOAD;
        todo_proc->time_slice = rq->max_time_slice;                                 // 运行时间 = 调度周期，即最长
	} else {                                                                        // 更新对应进程的 vruntime 值，vruntime = 实际运行时间 * 1024 / 进程权重 。
		todo_proc->lab6_stride += todo_proc->real_run_time * NICE_0_LOAD / todo_proc->lab6_priority;
        // 运行时间 = 调度周期 * 进程权重 / 所有进程权重之和
        todo_proc->time_slice = rq->max_time_slice * todo_proc->lab6_priority / rq->proc_priotrity_tot;
	}
	return todo_proc;
}

struct sched_class cfs_sched_class = {
     .name = "cfs_scheduler",
     .init = cfs_init,
     .enqueue = cfs_enqueue,
     .dequeue = cfs_dequeue,
     .pick_next = cfs_pick_next,
     .proc_tick = cfs_proc_tick,
};