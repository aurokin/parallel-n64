#include "worker_thread.hpp"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>

namespace
{
struct WorkItem
{
	int id = -1;
};

struct WorkState
{
	std::mutex lock;
	std::vector<int> performed;
	std::vector<int> notified;
	std::atomic_uint32_t notified_count = {0};
};

struct Executor
{
	std::shared_ptr<WorkState> state;

	bool is_sentinel(const WorkItem &item) const
	{
		return item.id < 0;
	}

	void perform_work(WorkItem &item)
	{
		std::lock_guard<std::mutex> holder{state->lock};
		state->performed.push_back(item.id);
	}

	void notify_work_locked(const WorkItem &item)
	{
		std::lock_guard<std::mutex> holder{state->lock};
		state->notified.push_back(item.id);
		state->notified_count++;
	}
};

static void check(bool condition, const char *message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << std::endl;
		std::exit(1);
	}
}
}

int main()
{
	auto state = std::make_shared<WorkState>();

	{
		RDP::WorkerThread<WorkItem, Executor> worker(Executor{ state });
		worker.push(WorkItem{ 1 });
		worker.push(WorkItem{ 2 });
		worker.push(WorkItem{ 3 });
		worker.wait([&]() {
			return state->notified_count.load() == 3u;
		});
	}

	{
		std::lock_guard<std::mutex> holder{state->lock};
		check(state->performed.size() == 3u, "performed size mismatch");
		check(state->notified.size() == 3u, "notified size mismatch");
		check(state->performed[0] == 1 && state->performed[1] == 2 && state->performed[2] == 3,
		      "perform_work order mismatch");
		check(state->notified[0] == 1 && state->notified[1] == 2 && state->notified[2] == 3,
		      "notify_work_locked order mismatch");
	}

	// Ensure clean shutdown when no work is queued.
	{
		RDP::WorkerThread<WorkItem, Executor> worker(Executor{ std::make_shared<WorkState>() });
	}

	std::cout << "emu_unit_worker_thread_test: PASS" << std::endl;
	return 0;
}
