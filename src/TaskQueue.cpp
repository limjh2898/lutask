#include <lutask/TaskQueue.h>

lutask::TaskQueue::TaskQueue(size_t maxTask)
	: maxTask_(maxTask)
{
}

lutask::TaskQueue::~TaskQueue()
{
}

bool lutask::TaskQueue::Push(ITask* task, bool force)
{
	if (maxTask_ < taskQueue_.unsafe_size() && force == false)
	{
		return false;
	}

	taskQueue_.push(task);
	return true;
}

bool lutask::TaskQueue::Pop(ITask*& out_task)
{
	if (taskQueue_.try_pop(out_task) == true)
	{
		return true;
	}

	return true;
}