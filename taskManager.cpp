#include "taskManager.h"

#include "taskQueue.h"
#include "bufferTool.h"
#include "js2.h"
#include "showWin.h"

TaskQueue JobQueue;
TaskQueue DoneQueue;

void cleanup() {
    // first empty anything on the job queue
    while(1) {
	TaskItem t = JobQueue.getNoWait();
	if (t.type == TaskItem::None)
	    break;
    }
    for (int i = 0; i < numThreads; ++i) {
	TaskItem t;
	t.type = TaskItem::Shutdown;
	JobQueue.add(t);
    }

    for (int i = 0; i < numThreads; ++i)
	threads[i].join();
}

int startLevel(unsigned int stepGroup, unsigned int step) {
    BufferId bId(stepGroup, step-1);
    int level = bufMan.levelCountInStep(bId) - 1;
    bId.mergeLevel = level;
    test(1 == bufMan.groupCountInLevel(bId), "highest merge level has other than 1 group");
    bId.group = 0;
    test(bufMan.groupFinished(bId), "group not finished in startLevel");
    BGroupStat bgStat;
    test(bufMan.getGroupStat(bId, &bgStat), "Can't get group stats in startLevel");

    uint64_t positions = bgStat.positions;
    if (positions == 0) {
	printf ("no solutions found on step %u!!!", step);
	return 0;
    }
    printf("level: %d positions: %" PRIu64 " at time: %ld\n",
	   step, positions, (long)(time(NULL) - startTime));
    uint64_t boardsPerTask = bgStat.positions / numThreads / 2;
    if (boardsPerTask < 20)
	boardsPerTask = 20;

    bId.buf = 0;
    BufferStat bStat;
    test(bufMan.getBufferStat(bId, &bStat), "Can't get buffer stats in startLevel");
    uint64_t bufPos = bStat.positions;
    uint64_t offset = 0;
    BufferId outBId(stepGroup, step);

    int processingJobs = 0;
    
    while(1) {
	if (boardsPerTask > positions)
	    boardsPerTask = positions;
	TaskItem t;
	t.type = TaskItem::ProcessPositions;
	t.process = TaskProcess(bId, offset, boardsPerTask, outBId);
	JobQueue.add(t);
	processingJobs++;
	positions -= boardsPerTask;
	if (positions == 0)
	    return processingJobs;
	offset += boardsPerTask;
	if (offset >= bufPos) {
	    offset -= bufPos;
	    bId.buf++;
	    test(bufMan.getBufferStat(bId, &bStat), "Can't get more buffer stats in startLevel");
	    bufPos = bStat.positions;
	}
    }
    // unreachable
    return -1;
}


void TaskManager::run() {
    time(&startTime);
    stepGroups = 1;
    curStep = 1;
    
    while(1) {
	for (int sg = 0; sg < stepGroups; ++sg) {
	    workStage[sg] = 1;
	    mergeLevelFinished[sg] = -1;
	    stragglerCount[sg] = 0;
	    processTasksOutstanding[sg] = startLevel(sg, curStep);
	}
	
	while(1) {
	    int pg = 0;
	    TaskItem t = DoneQueue.get();
	    if (t.type == TaskItem::ProcessPositions) {
		//printf("taskMan got ProcessPositions\n");
		pg = t.process.inp.stepGroup;
		processTasksOutstanding[pg]--;
	    } else if (t.type == TaskItem::CheckWins) {
		//printf("taskMan got CheckWins\n");
		pg = t.check.inp.stepGroup;
		processTasksOutstanding[pg]--;
	    } else if (t.type == TaskItem::FinishedGroup) {
		//printf("taskMan got FinishedGroup\n");
		pg = t.bufGroup.bId.stepGroup;
		// I suppose I could get hints for how to look for work
	    } else if (t.type == TaskItem::FoundWin) {
		//printf("taskMan got FoundWin\n");
		handleWin(t);
	    } else {
		//printf("taskMan got other item (%d)\n", (int)t.type);
	    }

	    if (lookForWork()) {
		curStep++;
		break; // no more work for this step
	    }
	}
    }
}


// see if there are any tasks to queue up.
// return 1 if all tasks for the current step are finished
int TaskManager::lookForWork() {
    int doneCount = 0;
    for (int sg = 0; sg < stepGroups; ++sg) {
	switch(workStage[sg]) {
	case 1:
	    lookForWork_stage_1(sg);
	    break;
	case 2:
	    lookForWork_stage_2(sg);
	    break;
	case 3:
	    lookForWork_stage_3(sg);
	    // stage 3 can push this to state 4.  Check for this
	    if (workStage[sg] == 4)
		++doneCount;
	    break;
	case 4:
	    ++doneCount;
	    break;
	}
	//printf("workStage %d\n", workStage[sg]);
    }
    if (doneCount == stepGroups){
	//printf("returning 1!!!\n");
	return 1;
    }
    return 0;
}

void TaskManager::lookForWork_stage_1(int sg) {
    uint16_t levelCount = bufMan.levelCountInStep(BufferId(sg, curStep));
    
    // we're limited in what we know until the initial processing of positions has been done
    // so we can't know if levels are finished or really deal with stragglers (because we don't
    // know if they're stragglers yet) so we have two modes of operation.
    
    // bufWorking keeps track of if there are any buffer groups still waiting to be written
    // at the current level or below.  Once there are 0 at the current level we can add
    // stragglers for that level and mark the level finished.
    char bufWorking = 0;
    if (processTasksOutstanding[sg])
	bufWorking = 1;
    for (int ml = mergeLevelFinished[sg] + 1; ml < levelCount; ++ml) {
	TaskItem ti;
	int ic = 0;  // input count
	uint32_t gc = bufMan.groupCountInLevel(BufferId(sg, curStep, ml));
	for (uint32_t g = 0; g < gc; ++g) {
	    BGroupStat bgStat;
	    bufMan.getGroupStat(BufferId(sg, curStep, ml, g), &bgStat);
	    if (bgStat.status == SEQ_WORKING)
		bufWorking = 1;
	    if (bgStat.status == SEQ_FINISHED) {
		ti.merge.inp[ic++] = BufferId(sg, curStep, ml, g);
		if (ic == TASKMERGE_MAX) { // do we have anything actionable?
		    ti.merge.out = BufferId(sg, curStep, ml+1);
		    test(bufMan.appendBufferGroup(&ti.merge.out), "can't appendbuffergroup in lookForWork");
		    ti.type = TaskItem::MergeGroups;
		    for (int i = 0; i < TASKMERGE_MAX; ++i)
			bufMan.setGroupStatus(ti.merge.inp[i], SEQ_MERGING);
		    JobQueue.add(ti);
		    ic = 0;
		}
	    }
	}
	
	if (!bufWorking) {  // we can finish off this level
	    for (uint32_t g = 0; g < gc; ++g) {
		if (bufMan.groupFinished(BufferId(sg, curStep, ml, g))) {
		    stragglerList[sg][stragglerCount[sg]++] = BufferId(sg, curStep, ml, g);
		    if (stragglerCount[sg] == TASKMERGE_MAX) {
			TaskItem ti;
			ti.type = TaskItem::MergeGroups;
			ti.merge.out = BufferId(sg, curStep, ml+1);
			test(bufMan.appendBufferGroup(&ti.merge.out), "can't appendbuffergroup in lookForWork");
			for (int i = 0; i < TASKMERGE_MAX; ++i) {
			    ti.merge.inp[i] = stragglerList[sg][i];
			    bufMan.setGroupStatus(ti.merge.inp[i], SEQ_MERGING);
			}
			JobQueue.add(ti);
			stragglerCount[sg] = 0;
		    }
		}
	    }
	    mergeLevelFinished[sg] = ml;
	}
    }
    


    // see if it's time to do the final straggler merge or dedup
    //
    // if all levels are "finished" then we're in the final steps.
    //
    // * if there's 0 groups on the straggler list then we've run out of positions!!!
    // * if there's 2+ groups on the straggler list then we're ready for the final straggler processing
    // * if there's 1 group on the straggler list then we're ready for the final dedup then search
    //   for a win

    if (processTasksOutstanding[sg])
	return;

    // get a new copy of level count because we might have just added a level
    levelCount = bufMan.levelCountInStep(BufferId(sg, curStep));
    if (mergeLevelFinished[sg] != levelCount-1)
	return;

    if (stragglerCount[sg] == 0) {
	printf("no positions generated!");
	cleanup();

	WorkerThread w;
	w.printBuffer(bufMan.finalBufId(sg, curStep-1));
	exit(0);
    } else if (stragglerCount[sg] > 1) {  // do the final straggler merge
	TaskItem ti;
	ti.type = TaskItem::MergeGroups;
	ti.merge.out = BufferId(sg, curStep, levelCount);
	test(bufMan.appendBufferGroup(&ti.merge.out), "can't appendbuffergroup in lookForWork final stragglers");
	for (int i = 0; i < TASKMERGE_MAX; ++i) {
	    if (i >= stragglerCount[sg]) {
		ti.merge.inp[i] = BufferId(0,0,0,0,0,1);
	    } else {
		ti.merge.inp[i] = stragglerList[sg][i];
		bufMan.setGroupStatus(ti.merge.inp[i], SEQ_MERGING);
	    }
	}
	
	JobQueue.add(ti);
	stragglerCount[sg] = 0;
    } else {
	// dedup the level
	workStage[sg] = 2;

	// let's make some assertions to validate that our other code isn't stupid.
	levelCount = bufMan.levelCountInStep(BufferId(sg, curStep));
	for (int ml = 0; ml < levelCount; ++ml) {
	    int gCount = bufMan.groupCountInLevel(BufferId(sg, curStep, ml));

	    // valid status could be FINISHED or DELETED but should soon move to
	    // DELETED for all but the highest merge level.
	    if (ml == levelCount - 1) {
		test(gCount == 1, "should be exactly one buffer group on the final merge level");
	    }
	    for (int g = 0; g < gCount; ++g) {
		BGroupStat bgStat;
		bufMan.getGroupStat(BufferId(sg, curStep, ml, g), &bgStat);
		if (bgStat.status != SEQ_DELETED && bgStat.status != SEQ_FINISHED && bgStat.status != SEQ_MERGING) {
		    printf("improper status (%d) of group before doing final merge\n", bgStat.status);
		    printf("bufferId: ");
		    BufferId(sg, curStep, ml, g).print();
		    printf("\n");
		    cleanup();
		    exit(0);
		}
		     
	    }
	}
	TaskItem ti;
	ti.type = TaskItem::DedupGroup;
	ti.dedup.inp = BufferId(sg, curStep, levelCount-1, 0);
	ti.dedup.out = BufferId(sg, curStep, levelCount, 0);
	ti.dedup.gen[0] = bufMan.finalBufId(BufferId(sg, curStep-1));
	if (curStep > 1) {
	    ti.dedup.gen[1] = bufMan.finalBufId(BufferId(sg, curStep-2));
	} else {
	    ti.dedup.gen[1] = BufferId(0,0,0,0,0,1);
	}
	bufMan.setGroupStatus(ti.dedup.inp, SEQ_MERGING);
	bufMan.startBufferGroup(ti.dedup.out, 1);
	JobQueue.add(ti);
    }
}

void TaskManager::lookForWork_stage_2(int sg) {
    // we're waiting for the final buffergroup to be labled "finished"
    if (!bufMan.groupFinished(bufMan.finalBufId(sg, curStep)))
	return;

    workStage[sg] = 3;

    // we have the final buffer group for the generation.  Divide it up into parts
    // and check for wins.
    BufferId bId = bufMan.finalBufId(BufferId(sg, curStep));
    BGroupStat bgStat;
    test(bufMan.getGroupStat(bId, &bgStat), "Can't get group stats in lookforwork_stage_2");

    uint64_t positions = bgStat.positions;
    if (positions == 0) {
	printf("0 positions in final merge step in stage_2\n");
	cleanup();
	WorkerThread w;
	w.printBuffer(bufMan.finalBufId(sg, curStep-1));
	exit(0);
    }

    //printf("*****STEP_POSITIONS Step %u: %lu positions\n", curStep, positions);
    
    
    uint64_t boardsPerTask = bgStat.positions / numThreads / 2;
    if (boardsPerTask < 20)
	boardsPerTask = 20;

    bId.buf = 0;
    BufferStat bStat;
    test(bufMan.getBufferStat(bId, &bStat), "Can't get buffer stats in stage_2");
    uint64_t bufPos = bStat.positions;
    uint64_t offset = 0;

    processTasksOutstanding[sg] = 0;
    
    while(1) {
	if (boardsPerTask > positions)
	    boardsPerTask = positions;
	TaskItem t;
	t.type = TaskItem::CheckWins;
	t.check = TaskCheck(bId, offset, boardsPerTask);
	JobQueue.add(t);
	processTasksOutstanding[sg]++;
	positions -= boardsPerTask;
	if (positions == 0)
	    break;
	offset += boardsPerTask;
	if (offset >= bufPos) {
	    offset -= bufPos;
	    bId.buf++;
	    test(bufMan.getBufferStat(bId, &bStat), "Can't get more buffer stats in startLevel");
	    bufPos = bStat.positions;
	}
    }

    if (winOnly) {
	// delete the buffer from 2 steps ago.  It is no longer needed
	if (curStep > 1) {
	    TaskItem t;
	    t.type = TaskItem::DeleteGroup;
	    t.del = TaskDelete(bufMan.finalBufId(sg, curStep-2));
	    JobQueue.add(t);
	}
    }
}

void TaskManager::lookForWork_stage_3(int sg) {
    if (processTasksOutstanding[sg] == 0)
	workStage[sg] = 4;
}

// what do we do when we find a winning position
void TaskManager::handleWin(TaskItem t) {
    printf("found win!!!\n");

    cleanup();

    if (winOnly) {
	WorkerThread w;
	w.printBuffer(t.foundWin.bId, t.foundWin.offset, 1);
    } else {
	showWin(t.foundWin.bId, t.foundWin.offset);
    }
    exit(0);
    
}



