#ifndef taskQueue_h
#define taskQueue_h

#include "bufferTool.h"
#include <queue>
#include <condition_variable>





struct TaskProcess {
    BufferId inp;
    uint64_t offset;
    uint64_t count;
    BufferId out;

    TaskProcess(BufferId inp_, uint64_t offset_, uint64_t count_, BufferId out_):
	inp(inp_), offset(offset_), count(count_), out(out_) { }
};

#define TASKMERGE_MAX 8

struct TaskMerge {
    BufferId inp[TASKMERGE_MAX];
    BufferId out;

    TaskMerge();
};

#define TASKDEDUP_MAX 2
struct TaskDedup {
    BufferId inp;
    BufferId out;
    BufferId gen[TASKDEDUP_MAX];

    TaskDedup(BufferId inp_, BufferId out_,
	      BufferId gen1, BufferId gen2=BufferId(0,0,0,0,0,1)):
	inp(inp_), out(out_) {
	gen[0] = gen1;
	gen[1] = gen2;
    }
};

struct TaskDelete {
    BufferId bId;

    TaskDelete(BufferId bId_): bId(bId_) {}
};

struct TaskCheck {
    BufferId inp;
    uint64_t offset;
    uint64_t count;

    TaskCheck(BufferId inp_, uint64_t offset_, uint64_t count_):
	inp(inp_), offset(offset_), count(count_) { }
};

struct TaskFinishedBufGroup {
    BufferId bId;

    TaskFinishedBufGroup(BufferId bId_): bId(bId_) { }
};

struct TaskFoundWin {
    BufferId bId;
    uint64_t offset;
    TaskFoundWin(BufferId bId_, uint64_t offset_): bId(bId_), offset(offset_) { }
};

struct TaskItem {
    enum {None,
	  ProcessPositions,  // jobs for worker threads
	  MergeGroups,
	  DedupGroup,
	  DeleteGroup,
	  CheckWins,
	  Shutdown,
	  FinishedGroup,    // extra responses from workers that the task manager can do something with
	  FoundWin,
    } type;
    
    union {
	TaskProcess process;
	TaskMerge merge;
	TaskDedup dedup;
	TaskDelete del;
	TaskCheck check;
	TaskFinishedBufGroup bufGroup;
	TaskFoundWin foundWin;
    };
    TaskItem(): type(None) { }
};


class TaskQueue {
private:
    std::queue<TaskItem> q;
    std::mutex m;
    std::condition_variable c;
public:
    // Add an element to the queue.
    void add(TaskItem t) {
	std::lock_guard<std::mutex> lock(m);
	q.push(t);
	c.notify_one();
    }

    // Get the "front"-element.
    // If the queue is empty, wait till a element is avaiable.
    TaskItem get(void) {
	std::unique_lock<std::mutex> lock(m);
	while(q.empty()) {
	    // release lock as long as the wait and reaquire it afterwards.
	    c.wait(lock);
	}
	TaskItem val = q.front();
	q.pop();
	return val;
    }
    
};

extern TaskQueue JobQueue;
extern TaskQueue DoneQueue;

    

   

#endif
