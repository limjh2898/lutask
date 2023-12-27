#pragma once

#ifdef WIN32
#include <concurrent_queue.h>
#endif
#include "ITask.h"

namespace lutask
{
	class TaskQueue final
	{
		const size_t DEFAULT_MAX_TASK = 1000000;

	public:
		TaskQueue(size_t maxTask = DEFAULT_MAX_TASK);
		~TaskQueue();

		bool Push(ITask* task, bool force = false);
		bool Pop(ITask*& out_task);

#ifdef WIN32
		const size_t Size() { return taskQueue_.unsafe_size(); }
	private:
		concurrency::concurrent_queue<ITask*> taskQueue_;
#else
	//	const size_t Size() { return taskQueue_.unsafe_size(); }
	//private:
	//	concurrency::concurrent_queue<ITask*> taskQueue_;
#endif
		size_t maxTask_;
	};
}