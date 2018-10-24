#ifndef taskManager_h
#define taskManager_h

#include "bufferTool.h"
#include "taskQueue.h"
#include <time.h>
extern time_t startTime;

class TaskManager {
    int stepGroups;
    int curStep;
    int processTasksOutstanding[2];
    int mergeLevelFinished[2];
    // "stragglers" are finished buffer groups remaining after full sets of TASKMERGE_MAX
    // buffer groups have been processed on that level and will be merged at higher levels
    BufferId stragglerList[2][TASKMERGE_MAX];  
    int stragglerCount[2];
    int workStage[2];  // 1: processing and merging of buffers
                       // 2: final deduping the level against previous generations
                       // 3: search for win
                       // 4: complete

public:
    void run();
private:
    void handleWin();
    int lookForWork();
    void lookForWork_stage_1(int sg);
    void lookForWork_stage_2(int sg);
    void lookForWork_stage_3(int sg);

};


#endif
