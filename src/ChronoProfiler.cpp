#include "ChronoProfiler.hpp"

/**
 * @file ChronoProfiler.cpp
 * @brief Implementation of ChronoProfiler w/ optional features...
 *
 * @details
 * ChronoProfiler is a lightweight, real-time CPU profiler that supports
 * per-thread, per-frame timing of named code zones.
 *
 * Optional features implemented here include:
 *  - Thread names for UI labeling
 *  - Event colors and categories for visualization
 *  - Per-thread ring buffers to limit memory usage
 *  - JSON export for offline analysis
 *  - Runaway event prevention and total event tracking
 *
 * @note The implementation uses standard C++ libraries:
 *  - '<chrono>' for high-resolution timing
 *  - '<vector>' and '<unordered_map>' for storage
 *  - '<thread>' and '<mutex>' for thread safety
 *  - '<atomic>' for atomic counters
 *  - '<fstream>' and '<iomanip>' for JSON export
 *  - 'nlohmann/json.hpp' (external) for JSON serialization
 *
 * @authors Eric Newton
 */

#include <fstream>           ///< std::ofstream for file output
#include <iomanip>           ///< std::setw for JSON format
#include <nlohmann/json.hpp> ///< external library for JSON export
#include <sstream>           ///< std::stringstream for string format

/** @brief Thread-local event list for each thread. Each thread writes to its
 * own vector to avoid locks. */
thread_local std::vector<ChronoProfiler::Event> ChronoProfiler::threadEvents;

/** @brief Merged event list for the completed frame. */
std::vector<ChronoProfiler::Event> ChronoProfiler::frameEvents;

/** @brief Mapping from thread IDs to human-readable names. */
std::unordered_map<uint32_t, std::string> ChronoProfiler::threadNames;

/** @brief Mutex protecting access to the threadNames map. */
std::mutex ChronoProfiler::threadNamesMutex;

/** @brief Mutex protecting merging of thread-local buffers into the frameEvents
 * vector. */
std::mutex ChronoProfiler::mergeMutex;

/** @brief Timestamp indicating the start of the current frame. */
std::chrono::high_resolution_clock::time_point ChronoProfiler::frameStart;

/** @brief Global atomic counter for total number of events recorded. */
std::atomic<size_t> ChronoProfiler::totalEventCount{0};

/** @brief Registry of all thread-local buffers for multi-thread merging. */
std::vector<std::vector<ChronoProfiler::Event> *>
    ChronoProfiler::allThreadBuffers;

/**
 * @brief Get current timestamp in milliseconds (since the epoch.
 * @return High-resolution timestamp in milliseconds (double-precision).
 *
 * @details Uses 'std::chrono::high_resolution_clock' to provide accurate timing
 *          for profiling events. Used internally to compute event durations.
 */
double ChronoProfiler::nowMs() {
  using namespace std::chrono;

  // return current time in milliseconds
  return duration<double, std::milli>(
             high_resolution_clock::now().time_since_epoch())
      .count();
}

/**
 * @brief Begin a new profiling frame.
 *
 * @details Should be called at the start of each frame (e.g., game/render loop).
 *          Clears previous frame events & resets the total event counter.
 *
 * @note Locks are used to safely clear the shared frameEvents vector.
 */
void ChronoProfiler::beginFrame() {
  frameStart =
      std::chrono::high_resolution_clock::now(); // record frame start time

  std::lock_guard<std::mutex> lock(mergeMutex); // protect shared frameEvents
  frameEvents.clear();
  totalEventCount = 0;
}

/**
 * @brief End the current profiling frame.
 *
 * @details Merges thread-local buffers from all threads into 'frameEvents'
 *          for the current frame. Each thread-local buffer is cleared after merging.
 *
 * @note Locking occurs here to ensure thread-safe merging.
 */
void ChronoProfiler::endFrame() {
  std::lock_guard<std::mutex> lock(mergeMutex); // ensure thread-safe merge

  for (auto *buffer : allThreadBuffers) {
    frameEvents.insert(frameEvents.end(), buffer->begin(), buffer->end());
    buffer->clear(); ///< clear thread-local buffer for next frame
  }
}

/**
 * @brief Start a profiling zone for the current thread.
 *
 * @param name Zone name (caller-owned string)
 * @param color Optional RGBA color for visualization (default=0x64C8FFFF)
 * @param category Optional string category for grouping zones in UI
 *
 * @details Creates an Event object and pushes it onto the thread-local vector.
 *          Duration is calculated when 'pushEventEnd()' is called.
 *
 * @note Each thread registers its local buffer only once using a
 *       thread-local static lambda.
 *
 * @note Prevents runaway growth using 'kMaxEventsPerThread'.
 */
void ChronoProfiler::pushEventStart(std::string_view name, uint32_t color,
                                    const std::string &category) {
  // register this thread's buffer only once
  static thread_local bool registered = []() -> bool {
    std::lock_guard<std::mutex> lock(mergeMutex);
    allThreadBuffers.push_back(&threadEvents);
    return true;
  }();

  if (threadEvents.size() >= kMaxEventsPerThread) {
    return; ///< prevent runaway growth
  }

  Event evt{};
  evt.name = name;
  evt.startMs = nowMs();
  evt.durationMs = -1.0; ///< duration unknown until pushEventEnd()
  evt.threadId = static_cast<uint32_t>(
      std::hash<std::thread::id>{}(std::this_thread::get_id()));
  evt.color = color;
  evt.category = category;

  threadEvents.push_back(evt); // store event in thread-local buffer
  totalEventCount++;           // increment global event counter
}

/**
 * @brief End the most recent profiling zone for the current thread.
 *
 * @details Calculates the duration of the zone and converts the start time
 *          to be relative to 'frameStart'.
 *
 * @note Does nothing if there are no events in the thread-local buffer.
 */
void ChronoProfiler::pushEventEnd() {
  if (threadEvents.empty())
    return;

  Event &evt = threadEvents.back();
  double endTimeMs = nowMs();
  evt.durationMs = endTimeMs - evt.startMs; // compute how long the event lasted

  // adjust start time to be relative to the beginning of the frame
  evt.startMs -=
      std::chrono::duration<double, std::milli>(frameStart.time_since_epoch())
          .count();
}

/**
 * @brief Retrieve all finalized events for the last completed frame.
 * @return Const reference to the vector of Event objects.
 *
 * @note Valid only until the next frame.
 */
const std::vector<ChronoProfiler::Event> &ChronoProfiler::getEvents() {
  return frameEvents;
}

/**
 * @brief Assign a human-readable name to the current thread.
 * @param name Thread name string
 *
 * @details Useful for labeling timeline tracks in visualizations.
 *          Stores mapping from thread ID to name in a thread-safe way.
 */
void ChronoProfiler::setThreadName(const std::string &name) {
  uint32_t id = static_cast<uint32_t>(
      std::hash<std::thread::id>{}(std::this_thread::get_id()));
  std::lock_guard<std::mutex> lock(threadNamesMutex);
  threadNames[id] = name; // map thread ID to name
}

/**
 * @brief Retrieve a human-readable name for a given thread ID.
 * @param threadId Numeric thread ID
 * @return Thread name string if set, otherwise "<unnamed>"
 */
std::string ChronoProfiler::getThreadName(uint32_t threadId) {
  // ensure thread-safe access to threadNames map
  std::lock_guard<std::mutex> lock(threadNamesMutex);
  auto it = threadNames.find(threadId); // look up the thread ID
  return (it != threadNames.end())
             ? it->second
             : "<unnamed>"; // return name if found, else "<unnamed>"
}

/**
 * @brief Export current frame events to a JSON file.
 * @param filename Path to output JSON file
 *
 * @details Each Event object is serialized with name, timestamps, duration,
 *          thread ID, thread name, color, and category.
 */
void ChronoProfiler::exportToJSON(const std::string &filename) {
  nlohmann::json j; // JSON array to store all frame events

  for (const auto &evt : frameEvents) {
    // add event details to the JSON array
    j.push_back({{"name", evt.name},
                 {"startMs", evt.startMs},
                 {"durationMs", evt.durationMs},
                 {"threadId", evt.threadId},
                 {"threadName", getThreadName(evt.threadId)},
                 {"color", evt.color},
                 {"category", evt.category}});
  }

  std::ofstream ofs(filename); // open the file for writing
  if (ofs.is_open()) {
    ofs << std::setw(2) << j << std::endl; // write formatted JSON to file
  }
}
