#include "duckdb/main/perfetto.hpp"
#include <chrono>
#include <cstring>
#include <fstream>
#include <string>
#include <unistd.h>
#include <iostream>
#include <mutex>
//---------------------------------------------------------------------------
#ifdef PERFETTO
//---------------------------------------------------------------------------
namespace duckdb {
//---------------------------------------------------------------------------
__thread PerfettoTracer::DurationEventChunk* localEvents;
//---------------------------------------------------------------------------
std::chrono::system_clock::time_point globalTime = std::chrono::system_clock::now();
//---------------------------------------------------------------------------
uint32_t now() {
	auto time = std::chrono::system_clock::now();
	return std::chrono::duration_cast<std::chrono::microseconds>(time - globalTime).count();
}
//---------------------------------------------------------------------------
static PerfettoTracer* getPerfettoTracer() {
	static PerfettoTracer tracer;
	return &tracer;
}
//---------------------------------------------------------------------------
PerfettoTracer::~PerfettoTracer()
// The destructor
{
	dump("perfetto.trace");
}
//---------------------------------------------------------------------------
void PerfettoTracer::registerThread(std::string&& threadName)
// Register a thread
{
	auto tracer = getPerfettoTracer();
	std::unique_lock lock{tracer->mutex};
	localEvents = static_cast<DurationEventChunk*>(std::malloc(sizeof(DurationEventChunk)));
	localEvents->next = nullptr;
	localEvents->index = 0;
	tracer->threadInfo.emplace_back(ThreadInfo{static_cast<size_t>(gettid()), move(threadName)});
	tracer->events.push_back(localEvents);
}
//---------------------------------------------------------------------------
PerfettoTracer::Trace::Trace(std::string_view event) : name(event), begin(now())
// The constructor
{
}
//---------------------------------------------------------------------------
PerfettoTracer::Trace::Trace(std::string&& event) : begin(now())
// The constructor
{
	auto ev = move(event);
	auto* data = static_cast<char*>(std::malloc(ev.size()));
	ev.copy(data, ev.size());
	name = {data, ev.size()};
}
//---------------------------------------------------------------------------
static std::once_flag perfettoWarning;
//---------------------------------------------------------------------------
PerfettoTracer::Trace::~Trace()
// The destructor
{
	if (!localEvents) {
		std::call_once(perfettoWarning, [&]() {
			std::cerr << "did you forget to call PerfettoTracer::registerThread()?" << "\n";
		});
		return;
	}
	auto end = now();
	if (localEvents->index == DurationEventChunk::maxEvents) {
		localEvents->next = static_cast<DurationEventChunk*>(std::malloc(sizeof(DurationEventChunk)));
		localEvents = localEvents->next;
		localEvents->next = nullptr;
		localEvents->index = 0;
	}
	localEvents->events[localEvents->index] = {name.data(), name.length(), begin, end};
	localEvents->index++;
}
//---------------------------------------------------------------------------
void PerfettoTracer::dump(std::string_view filename)
// Dump the perfetto trace
{
	std::cout << "Writing trace file ..." << "\n";

	std::ofstream trace;
	trace.open(std::string{filename});

	trace << '[' << '\n';

	auto pid = getpid();

	for (auto& thread : threadInfo) {
		trace << R"({"name":"thread_name","cat":"P","ph":"M","pid":)" << pid << R"(,"tid":)" << thread.tid << R"(,"args":{"name":")" << thread.name << R"("}},)" << '\n';
	}

	for (auto i = 0; i < events.size(); i++) {
		auto tid = threadInfo[i].tid;
		auto ev = events[i];
		while (ev) {
			for (auto j = 0; j < ev->index; j++) {
				auto event = ev->events[j];
				trace << R"({"name":")" << std::string_view(event.namePtr, event.nameLen) << R"(","cat":"P","ph":"B","pid":)" << pid << R"(,"tid":)" << tid <<  R"(,"ts":)" << event.begin << R"(},)" << '\n';
				trace << R"({"name":")" << std::string_view(event.namePtr, event.nameLen) << R"(","cat":"P","ph":"E","pid":)" << pid << R"(,"tid":)" << tid <<  R"(,"ts":)" << event.end << R"(},)" << '\n';
			}
			ev = ev->next;
		}
	}

	// empty value at end of list for valid json
	trace << "{}" << "\n";

	// and close the list
	trace << "]" << "\n";

	trace.close();
}
//---------------------------------------------------------------------------
}
//---------------------------------------------------------------------------
#endif
//---------------------------------------------------------------------------
