#pragma once
//---------------------------------------------------------------------------
#include <vector>
#include <string>
#include <string_view>
#include <mutex>
//---------------------------------------------------------------------------
namespace duckdb {
//---------------------------------------------------------------------------
using namespace std::string_view_literals;
//---------------------------------------------------------------------------
#define PERFETTO true
//---------------------------------------------------------------------------
#ifdef PERFETTO
//---------------------------------------------------------------------------
class PerfettoTracer {
	struct ThreadInfo {
		/// The thread ID
		size_t tid;
		/// The thread name
		std::string name;
	};

	/// The mutex
	std::mutex mutex;
	/// Information about the threads
	std::vector<ThreadInfo> threadInfo;

public:
	/// A duration event
	struct [[gnu::packed]] DurationEvent {
		/// Name of the event
		const char* namePtr;
		/// Length of the name
		size_t nameLen;
		/// Begin of the event
		uint32_t begin;
		/// End of the event
		uint32_t end;
	};

	/// A chunk of duration events
	struct DurationEventChunk {
		/// Events in a chunk
		static constexpr unsigned maxEvents = 682;

		/// The next chunk
		DurationEventChunk* next;
		/// The duration events
		DurationEvent events[maxEvents];
		/// Index of the next free slot
		uint16_t index;
	};

	/// Thread local event lists
	std::vector<DurationEventChunk*> events;

	/// The constructor
	explicit PerfettoTracer() = default;
	/// The destructor
	~PerfettoTracer();

	/// Register a thread
	static void registerThread(std::string&& threadName);

	/// Dump the perfetto trace
	void dump(std::string_view filename);

	/// Helper class for duration event tracing
	class Trace {
		/// Name of the event
		std::string_view name;
		/// Begin of the event
		uint32_t begin;

	public:
		/// The constructor
		explicit Trace(std::string_view event);
		/// The constructor
		explicit Trace(std::string&& event);
		/// The destructor
		~Trace();
	};
};
//---------------------------------------------------------------------------
#else
//---------------------------------------------------------------------------
class PerfettoTracer {
public:
	/// Register a thread
	static void registerThread(std::string&& /*threadName*/) {};

	/// Dump the perfetto trace
	void dump(std::string_view /*filename*/) {};

	/// Helper class for duration events
	class Trace {
	public:
		/// The constructor
		explicit Trace(std::string_view /*event*/) {};
		/// The destructor
		~Trace() = default;
	};
};
//---------------------------------------------------------------------------
#endif
//---------------------------------------------------------------------------
}
//---------------------------------------------------------------------------