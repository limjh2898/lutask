#pragma once

//#include <vector>
//#include <thread>
//#include <mutex>
//#include <queue>
//
//namespace lutask
//{
//class ITask;
//class TaskQueue
//{
//public:
//	TaskQueue() = default;
//	TaskQueue(const TaskQueue& )
//
//private:
//	std::queue<ITask*> q_;
//	std::mutex m_;
//};
//
//class ThreadPool 
//{
//public:
//	ThreadPool(size_t numThread);
//	~ThreadPool();
//
//	bool PushTask(ITask* task);
//
//private:
//
//
//private:
//	size_t numThread_;
//	std::vector<std::thread> workers_;
//	TaskQueue taskQueue_;
//
//	std::condition_variable cv_;
//	std::mutex m_;
//
//	bool terminated_;
//};
//
//}