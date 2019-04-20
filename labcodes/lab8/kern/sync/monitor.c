#include <stdio.h>
#include <monitor.h>
#include <kmalloc.h>
#include <assert.h>


// Initialize monitor.
void     
monitor_init (monitor_t * mtp, size_t num_cv) {
    int i;
    assert(num_cv>0);
    mtp->next_count = 0;
    mtp->cv = NULL;
    sem_init(&(mtp->mutex), 1); //unlocked
    sem_init(&(mtp->next), 0);
    mtp->cv =(condvar_t *) kmalloc(sizeof(condvar_t)*num_cv);
    assert(mtp->cv!=NULL);
    for(i=0; i<num_cv; i++){
        mtp->cv[i].count=0;
        sem_init(&(mtp->cv[i].sem),0);
        mtp->cv[i].owner=mtp;
    }
}

// Unlock one of threads waiting on the condition variable. 
void 
cond_signal (condvar_t *cvp) {
   //LAB7 EXERCISE1: YOUR CODE
   cprintf("cond_signal begin: cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);  
  /*
   *      cond_signal(cv) {
   *          if(cv.count>0) {
   *             mt.next_count ++;
   *             signal(cv.sem);
   *             wait(mt.next);
   *             mt.next_count--;
   *          }
   *       }
   */

    // LAB7 EXERCISE1: 2016011396
    if (cvp->count > 0) {               // 当前存在等待的进程
        cvp->owner->next_count ++;      // 等待的进程总数加一
        up(&(cvp->sem));                // 唤醒等待在cv.sem上等待的进程
        down(&(cvp->owner->next));      // 把自己等待
        cvp->owner->next_count --;      // 自己被唤醒后，等待的进程总数减一
    }
   cprintf("cond_signal end: cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
}

// Suspend calling thread on a condition variable waiting for condition Atomically unlocks 
// mutex and suspends calling thread on conditional variable after waking up locks mutex. Notice: mp is mutex semaphore for monitor's procedures
void
cond_wait (condvar_t *cvp) {
    //LAB7 EXERCISE1: YOUR CODE
    cprintf("cond_wait begin:  cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
   /*
    *         cv.count ++;
    *         if(mt.next_count>0)
    *            signal(mt.next)
    *         else
    *            signal(mt.mutex);
    *         wait(cv.sem);
    *         cv.count --;
    */

    // LAB7 EXERCISE1: 2016011396
    cvp->count ++;                      // 等待此条件的睡眠进程个数 cv.count 要加一
    if(cvp->owner->next_count > 0) {    // 有大于等于 1 个进程执行 cond_signal 函数
        up(&(cvp->owner->next));        // 唤醒进程链表中的下一个进程
    } else {                            // 没有进程执行 cond_signal 函数且等待了
        up(&(cvp->owner->mutex));       // 否则唤醒睡在 monitor.mutex 上的进程
    }
    down(&(cvp->sem));                  // 将自己睡眠
    cvp->count --;                      // 等待此条件的睡眠进程个数 cv.count 要减一

    cprintf("cond_wait end:  cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
}
