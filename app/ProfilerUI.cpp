#if defined(PROFILER)

#include "ProfilerUI.hpp"

/**
 * @file ProfilerUI.cpp
 * @brief Implementation of the ProfilerUI class for console-based visualization
 *        of ChronoProfiler CPU profiling events.
 *
 * This file contains the implementation of ProfilerUI methods, responsible for:
 *  - Storing frame history
 *  - Aggregating zone statistics
 *  - Rendering a simple ASCII-based profiler view in the terminal
 *
 * This implementation only exists when compiled with '-DPROFILER'.
 * Otherwise, the header provides a zero-overhead stub implementation.
 */

/* Required includes:
   <mutex>         - ensures thread-safety when UI reads profiling data
   <iomanip>       - provides formatting helpers like std::setw and std::setprecision
   <iostream>      - required to print profiler results to terminal
   <string>        - for converting zone names to std::string
   <vector>        - storage container for per-frame event history */
#include <mutex>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

/**
 * @brief Construct a new ProfilerUI object
 *
 * Allocates storage for the rolling frame history used in UI rendering.
 *
 * @param historySize Maximum number of frames to store in rolling history
 *                    (older frames automatically pop off the front)
 */
ProfilerUI::ProfilerUI(size_t historySize)
    : maxHistory(historySize), totalFrames(0) {
    frameHistory.reserve(historySize);
}

/**
 * @brief Update the profiler UI with the latest frame events
 *
 * This function should be called **once per frame**, immediately after
 * 'ChronoProfiler::endFrame()'. It:
 *  - Retrieves merged event data for the most recent frame
 *  - Stores a copy into rolling history
 *  - Manages max history size
 *  - Updates aggregated statistics for zone durations
 */
void ProfilerUI::update() {
    std::lock_guard<std::mutex> lock(uiMutex);  ///< ensure thread safety

    const auto& events = ChronoProfiler::getEvents(); ///< profiler provides merged events

    frameHistory.push_back(events);

    if (frameHistory.size() > maxHistory) {
        frameHistory.erase(frameHistory.begin()); ///< drop oldest frame
    }

    /* aggregate stats per named profiling zone */
    for (const auto& e : events) {
        aggregatedStats[std::string(e.name)].add(e.durationMs);
    }

    /* increment total frames */
    totalFrames++;
}

/**
 * @brief Render profiler result summary and latest frame visualization
 *
 * Produces terminal output such as:
 *
 * '''
 * === Frame 140 ===
 * updateScene        ████████████ 1.52 ms [MainThread]
 * drawFrame          ████████████████████████ 3.40 ms [RenderThread]
 *
 * -- Aggregated Stats --
 * Zone                 Avg(ms)    Max(ms)    Count
 * updateScene          1.50       2.02        140
 * drawFrame            3.38       4.02        140
 * '''
 */
void ProfilerUI::render() {
    std::lock_guard<std::mutex> lock(uiMutex);  // ensure UI rendering is thread-safe

    size_t frameIndex = totalFrames; ///< use absolute frame count

    // print current frame header
    std::cout << "\r=== Frame " << frameIndex << " ===\n" << std::flush;

    // render most recent frame's events (if available)
    if (!frameHistory.empty()) {
        renderFrame(frameHistory.back(), frameIndex);
    }

    // display aggregated timing statistics
    renderAggregatedStats();
}

/**
 * @brief Render all zones in a frame as ASCII timing bars
 *
 * @param events      Collection of profiling events for the frame
 * @param frameIndex  Absolute frame number (only used for labeling)
 */
void ProfilerUI::renderFrame(const std::vector<ChronoProfiler::Event>& events,
                             size_t frameIndex) {

    /* loop through all events recorded for this frame */
    for (const auto& e : events) {
        int barLength = static_cast<int>(e.durationMs * 10); ///< scale duration into bar length

        /* print event name, left-aligned */
        std::cout << std::setw(20) << std::left << e.name << " ";

        /* draw simple ASCII bar visualization of duration */
        for (int i = 0; i < barLength; ++i)
            std::cout << "█";

        /* print duration and thread name */
        std::cout << " "
                  << std::fixed << std::setprecision(2)
                  << e.durationMs << " ms"
                  << " [" << ChronoProfiler::getThreadName(e.threadId) << "]\n";
    }
}

/**
 * @brief Render aggregated statistics for all profiling zones
 *
 * Outputs a compact table displaying:
 *  - Zone name
 *  - Average duration
 *  - Maximum duration
 *  - Count of occurrences
 */
void ProfilerUI::renderAggregatedStats() {
    std::cout << "\n-- Aggregated Stats --\n";

    /* print table header (column labels) */
    std::cout << std::setw(20) << "Zone"
              << std::setw(10) << "Avg(ms)"
              << std::setw(10) << "Max(ms)"
              << std::setw(10) << "Count\n";

    /* iterate over aggregated stats and print each profiling zone's data */
    for (const auto& [name, stats] : aggregatedStats) {
        std::cout << std::setw(20) << name
                  << std::setw(10) << std::fixed << std::setprecision(2) << stats.avg()
                  << std::setw(10) << stats.maxMs
                  << std::setw(10) << stats.count << "\n";
    }
}

#endif // PROFILER