#pragma once

namespace lutask
{

enum class ETaskResult
{
	Wait,
	Continue,
	End,
	Fail
};

class ITask
{
public:
	virtual ~ITask() {}
	virtual ETaskResult Proc() = 0;
	virtual ITask& Wait() = 0;
	virtual uint64_t GetId() = 0;
};

class TaskBase : public ITask
{
public:
	TaskBase() : taskId(s_seed.fetch_add(1)) {}

	virtual ETaskResult Proc() override {}
	uint64_t GetId() override { return taskId; }

private:
	static std::atomic_uint64_t s_seed;
	uint64_t taskId;
};

}