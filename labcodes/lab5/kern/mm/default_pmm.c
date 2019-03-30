#include <pmm.h>
#include <list.h>
#include <string.h>
#include <default_pmm.h>

/*  In the First Fit algorithm, the allocator keeps a list of free blocks
 * (known as the free list). Once receiving a allocation request for memory,
 * it scans along the list for the first block that is large enough to satisfy
 * the request. If the chosen block is significantly larger than requested, it
 * is usually splitted, and the remainder will be added into the list as
 * another free block.
 *  Please refer to Page 196~198, Section 8.2 of Yan Wei Min's Chinese book
 * "Data Structure -- C programming language".
*/
// LAB2 EXERCISE 1: YOUR CODE
// you should rewrite functions: `default_init`, `default_init_memmap`,
// `default_alloc_pages`, `default_free_pages`.
/*
 * Details of FFMA
 * (1) Preparation:
 *  In order to implement the First-Fit Memory Allocation (FFMA), we should
 * manage the free memory blocks using a list. The struct `free_area_t` is used
 * for the management of free memory blocks.
 *  First, you should get familiar with the struct `list` in list.h. Struct
 * `list` is a simple doubly linked list implementation. You should know how to
 * USE `list_init`, `list_add`(`list_add_after`), `list_add_before`, `list_del`,
 * `list_next`, `list_prev`.
 *  There's a tricky method that is to transform a general `list` struct to a
 * special struct (such as struct `page`), using the following MACROs: `le2page`
 * (in memlayout.h), (and in future labs: `le2vma` (in vmm.h), `le2proc` (in
 * proc.h), etc).
 * (2) `default_init`:
 *  You can reuse the demo `default_init` function to initialize the `free_list`
 * and set `nr_free` to 0. `free_list` is used to record the free memory blocks.
 * `nr_free` is the total number of the free memory blocks.
 * (3) `default_init_memmap`:
 *  CALL GRAPH: `kern_init` --> `pmm_init` --> `page_init` --> `init_memmap` -->
 * `pmm_manager` --> `init_memmap`.
 *  This function is used to initialize a free block (with parameter `addr_base`,
 * `page_number`). In order to initialize a free block, firstly, you should
 * initialize each page (defined in memlayout.h) in this free block. This
 * procedure includes:
 *  - Setting the bit `PG_property` of `p->flags`, which means this page is
 * valid. P.S. In function `pmm_init` (in pmm.c), the bit `PG_reserved` of
 * `p->flags` is already set.
 *  - If this page is free and is not the first page of a free block,
 * `p->property` should be set to 0.
 *  - If this page is free and is the first page of a free block, `p->property`
 * should be set to be the total number of pages in the block.
 *  - `p->ref` should be 0, because now `p` is free and has no reference.
 *  After that, We can use `p->page_link` to link this page into `free_list`.
 * (e.g.: `list_add_before(&free_list, &(p->page_link));` )
 *  Finally, we should update the sum of the free memory blocks: `nr_free += n`.
 * (4) `default_alloc_pages`:
 *  Search for the first free block (block size >= n) in the free list and reszie
 * the block found, returning the address of this block as the address required by
 * `malloc`.
 *  (4.1)
 *      So you should search the free list like this:
 *          list_entry_t le = &free_list;
 *          while((le=list_next(le)) != &free_list) {
 *          ...
 *      (4.1.1)
 *          In the while loop, get the struct `page` and check if `p->property`
 *      (recording the num of free pages in this block) >= n.
 *              struct Page *p = le2page(le, page_link);
 *              if(p->property >= n){ ...
 *      (4.1.2)
 *          If we find this `p`, it means we've found a free block with its size
 *      >= n, whose first `n` pages can be malloced. Some flag bits of this page
 *      should be set as the following: `PG_reserved = 1`, `PG_property = 0`.
 *      Then, unlink the pages from `free_list`.
 *          (4.1.2.1)
 *              If `p->property > n`, we should re-calculate number of the rest
 *          pages of this free block. (e.g.: `le2page(le,page_link))->property
 *          = p->property - n;`)
 *          (4.1.3)
 *              Re-caluclate `nr_free` (number of the the rest of all free block).
 *          (4.1.4)
 *              return `p`.
 *      (4.2)
 *          If we can not find a free block with its size >=n, then return NULL.
 * (5) `default_free_pages`:
 *  re-link the pages into the free list, and may merge small free blocks into
 * the big ones.
 *  (5.1)
 *      According to the base address of the withdrawed blocks, search the free
 *  list for its correct position (with address from low to high), and insert
 *  the pages. (May use `list_next`, `le2page`, `list_add_before`)
 *  (5.2)
 *      Reset the fields of the pages, such as `p->ref` and `p->flags` (PageProperty)
 *  (5.3)
 *      Try to merge blocks at lower or higher addresses. Notice: This should
 *  change some pages' `p->property` correctly.
 */
free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

// LAB2 EXERCISE 1: 2016011396
// 我重写了 default_alloc_pages 和 default_free_pages 函数

static void
default_init(void) {
    list_init(&free_list);
    nr_free = 0;
}

static void
default_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);                      // n 应该大于 0，在 n <= 0 的情况下，ucore 会迅速报错。
    struct Page *p = base;              // 空闲页起始地址
    for (; p != base + n; p ++) {       // 对于该块中的页依次进行遍历
        assert(PageReserved(p));        // 检查该页面是否为预留页面
        p->flags = p->property = 0;     // 标记该 page 属于 free_list，并且不是第一个页
        set_page_ref(p, 0);             // ref 表示这页被页表的引用记数，初始化为 0
    }
    base->property = n;                 // 设置这个块的空闲页数为 n 

    // property 位仅当该 Page 处于 free_list 中时有效，代表了该 block 中闲置页的个数（包括当前页），而 flags 中的 property 位则被主要用作判断页是否已经被占用。
    // SetPageProperty 将其设置为1，表示这页是free的，可以被分配；ClearPageProperty 设置为0，表示这页已经被分配出去
    SetPageProperty(base);             // SetPageProperty(p) 将对第一个页面进行标记，将 p 的 flags 中的 property 位置设置为 1，这个不是 p->property，我对于所有页面进行了标记
    nr_free += n;                      // 空闲的页数加 n
    list_add(&free_list, &(base->page_link));   // 将本块的第一个页插入空闲内存链表，其 base-> property 记录了这个块空闲页面的大小，所以只用存储第一个页到链表中
}

// static struct Page *
// default_alloc_pages(size_t n) {
//     assert(n > 0);
//     if (n > nr_free) {
//         return NULL;
//     }
//     struct Page *page = NULL;
//     list_entry_t *le = &free_list;
//     while ((le = list_next(le)) != &free_list) {
//         struct Page *p = le2page(le, page_link);
//         if (p->property >= n) {
//             page = p;
//             break;
//         }
//     }
//     if (page != NULL) {
//         list_del(&(page->page_link));
//         if (page->property > n) {
//             struct Page *p = page + n;
//             p->property = page->property - n;
//             list_add(&free_list, &(p->page_link));
//     }
//         nr_free -= n;
//         ClearPageProperty(page);
//     }
//     return page;
// }

static struct Page *
default_alloc_pages(size_t n) {
    assert(n > 0);                      // n 应该大于 0，在 n <= 0 的情况下，ucore 会迅速报错。
    if (n > nr_free) {                  // n 大于总空闲页面的大小，则无法进行分配，直接返回 NULL。
        return NULL;
    }
    struct Page *page = NULL;
    list_entry_t *le = &free_list;

    // 第一步：遍历空闲内存块链表，找到第一个长度大于等于 n 的块
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);    // 运用宏将 list entry 转为 page
        if (p->property >= n) {         // 如果该块的页面数大于 n
            page = p;                   // 则找到了该块，跳出块搜索循环
            break;
        }
    }

    // 第二步：如果以及找到长度大于等于 n 的块，则进行如下分类判断：
    // 如果长度大于 n，在从这个块中取走长度为 n 的存储空间，并将剩下的存储空间重新插入到列表中
    // 如果长度等于 n 或者以及完成了块的切分插入，则直接删除原来的块
    if (page != NULL) {
        if (page->property > n) {                   // 将块切分并且将剩下的部分重新添加到链表中
            struct Page *p = page + n;              // 进行块的切分
            p->property = page->property - n;       // 计算剩余块的空闲页面数
            SetPageProperty(p);                     // 对于当前新的块头进行标记，表示这页是free的，可以被分配；
            list_add_after(&(page->page_link), &(p->page_link));    // 将新的块插入到原来块的后面
        }
        ClearPageProperty(page);                    // 表示本页当前被占用，如果设置为1，表示这页是free的，可以被分配；如果设置为0，表示这页已经被分配出去
        list_del(&(page->page_link));               // 删除原来的块，这样就保证了空闲链表的顺序
        nr_free -= n;                               // 空闲页数减去 n
    } 
    return page;
}

// static void
// default_free_pages(struct Page *base, size_t n) {
//     assert(n > 0);
//     struct Page *p = base;
//     for (; p != base + n; p ++) {
//         assert(!PageReserved(p) && !PageProperty(p));
//         p->flags = 0;
//         set_page_ref(p, 0);
//     }
//     base->property = n;
//     SetPageProperty(base);
//     list_entry_t *le = list_next(&free_list);
//     while (le != &free_list) {
//         p = le2page(le, page_link);
//         le = list_next(le);
//         if (base + base->property == p) {
//             base->property += p->property;
//             ClearPageProperty(p);
//             list_del(&(p->page_link));
//         }
//         else if (p + p->property == base) {
//             p->property += base->property;
//             ClearPageProperty(base);
//             base = p;
//             list_del(&(p->page_link));
//         }
//     }
//     nr_free += n;
//     list_add(&free_list, &(base->page_link));
// }

static void
default_free_pages(struct Page *base, size_t n) {
    assert(n > 0);                      // n 应该大于 0，在 n <= 0 的情况下，ucore 会迅速报错。
    struct Page *p = base;

    // 第一步：遍历该块中每一个页
    for (; p != base + n; p ++) {
        assert(!PageReserved(p) && !PageProperty(p));   // 检查块中的各个页的 p->flags 中的信息是否合法，本页应该不是被预留的并且被占用
        p->flags = 0;                                   // 标记其在 free_list 中
        set_page_ref(p, 0);                             // 设置其被页表引用记数为 0
    }

    // 第二步：设置好第一页的信息
    base->property = n;         // 当前空闲块中的页数为 n
    SetPageProperty(base);      // 对于当前块头进行标记，表示这页是free的，可以被分配；

    // 第三步：遍历空闲内存链表，找到该释放块应处的位置，原链表已按照地址从小到小大排序（双向链表）
    // 用 le 和 le_prev 将 base 夹在中间，然后将 base 插入即可
    list_entry_t *le = list_next(&free_list);
    list_entry_t *le_prev = &free_list;
    while (le != &free_list) {
        p = le2page(le, page_link);    // 运用宏将 list entry 转为 page

        // 如果第一次发现 p > base，就代表找到了，跳出循环，此时 le_prev < base < le 或者 le_prev = &free_list, base < le
        if (p > base) {
            break;
        } else {            // 否则继续向前搜索
            le_prev = le;
            le = list_next(le);
        }
    }

    // 第四步：检查 base 是否可以和链表前一项合并，此时 le_prev < base < le 或者 le_prev = &free_list, base < le
    // 只有 le_prev < base < le，le_prev != &free_list 情况下并且可和 le_prev 合并才不用将 base 块插入链表中
    p = le2page(le_prev, page_link);
    if (le_prev != &free_list && p + p->property == base) { // le_prev < base < le 并且可以合并情况
        p->property = p->property + base->property;         // 更新合并后的总空闲页面
        base->property = 0;                                 // 合并后 base 不再是块头，不用记录块内页面数信息
        ClearPageProperty(base);                            // 这一步为什么必要？
    } else {                                                // 其余情况将其插入 le_prev 之后即可
        list_add_after(le_prev, &(base->page_link));
        p = base;
    }

    // 第五步：检查 le 前一项是否可以和 le 合并，此时 le_prev < base < le 或者 le_prev > base(本就为头) < le
    // 考虑 p 和后项 le 合并的情况，如果前项已经合并了，p = le_prev，否则 p = base
    struct Page *p2 = le2page(le, page_link);
    if (le != &free_list && p + p->property == p2) {    // le == free_list 也会跳出循环，所以要判定
        p->property = p->property + p2->property;       // 更新合并后的总空闲页面
        p2->property = 0;                               // 合并后 le 不再是块头，不用记录块内页面数信息
        ClearPageProperty(p2);                          // 这一步为什么必要？
        list_del(le);                                   // 合并后项后把 le 块删除
    }

    // 第六步：将总空闲页 nr_free 数目加 n
    nr_free += n;                                       // 总空闲页面数加 n
}

static size_t
default_nr_free_pages(void) {
    return nr_free;
}

static void
basic_check(void) {
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);
    assert(nr_free == 3);

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    assert(!list_empty(&free_list));

    struct Page *p;
    assert((p = alloc_page()) == p0);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    free_list = free_list_store;
    nr_free = nr_free_store;

    free_page(p);
    free_page(p1);
    free_page(p2);
}

// LAB2: below code is used to check the first fit allocation algorithm (your EXERCISE 1) 
// NOTICE: You SHOULD NOT CHANGE basic_check, default_check functions!
static void
default_check(void) {
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count ++, total += p->property;
    }
    assert(total == nr_free_pages());

    basic_check();

    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    free_pages(p0 + 2, 3);
    assert(alloc_pages(4) == NULL);
    assert(PageProperty(p0 + 2) && p0[2].property == 3);
    assert((p1 = alloc_pages(3)) != NULL);
    assert(alloc_page() == NULL);
    assert(p0 + 2 == p1);

    p2 = p0 + 1;
    free_page(p0);
    free_pages(p1, 3);
    assert(PageProperty(p0) && p0->property == 1);
    assert(PageProperty(p1) && p1->property == 3);

    assert((p0 = alloc_page()) == p2 - 1);
    free_page(p0);
    assert((p0 = alloc_pages(2)) == p2 + 1);

    free_pages(p0, 2);
    free_page(p2);

    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count --, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
}

const struct pmm_manager default_pmm_manager = {
    .name = "default_pmm_manager",
    .init = default_init,
    .init_memmap = default_init_memmap,
    .alloc_pages = default_alloc_pages,
    .free_pages = default_free_pages,
    .nr_free_pages = default_nr_free_pages,
    .check = default_check,
};

