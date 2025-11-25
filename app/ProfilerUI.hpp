#pragma once

/**
 * @file ProfilerUI.hpp
 * @brief Console-based ASCII UI for visualizing ChronoProfiler results.
 *
 * This header declares a minimal, dependency-free ASCII profiler UI that:
 *  - maintains a rolling history of frames,
 *  - prints a per-frame breakdown as ASCII bars,
 *  - accumulates aggregated statistics (avg/max/count) per zone,
 *  - exposes a no-op implementation when the profiler is disabled so call
 *    sites can remain free of '#ifdef' guards.
 *
 * The real implementation is compiled only when the preprocessor symbol
 * 'PROFILER' is defined (e.g. pass '-DPROFILER' or 'make PROFILING=1').
 *
 * @note The UI is intentionally lightweight: it does not depend on ncurses,
 *       ImGui, or other GUI libraries. It is intended for quick in-terminal
 *       inspection during development.
 *
 * @author Eric Newton
 * @version 1.1
 * @date 2025-11-10
 * @since 1.0
 */

/* Toggle: compile real UI only when PROFILER is defined. */
#if defined(PROFILER)

// ---------------------------- //
// Real implementation includes //
// ---------------------------- //

// ChronoProfiler.hpp
// - Provides ChronoProfiler::Event and ChronoProfiler::getEvents()
// - Must be included so the UI can read the per-frame event list.
#include <ChronoProfiler.hpp>

// Standard-library includes with comments explaining why they're needed.
//
// <vector>         : store rolling frame history (vector< vector<Event> >)
// <string>         : keys for aggregatedStats and zone names
// <unordered_map>  : mapping zone name -> ZoneStats for O(1) lookups
// <mutex>          : protect update()/render() from multi-threaded access
// <iostream>       : print ASCII UI to stdout/stderr
// <iomanip>        : formatting width, precision for table columns
// <cstdint>        : fixed-size integer aliases (uint32_t) used by events
#include <vector>         // rolling history container
#include <string>         // zone names, thread names
#include <unordered_map>  // aggregate stats map
#include <mutex>          // thread-safety for update/render
#include <iostream>       // console output
#include <iomanip>        // formatting numeric/column widths
#include <cstdint>        // integer typedefs

/**
 * @struct ZoneStats
 * @brief Aggregated statistics for a given profiling zone.
 *
 * ZoneStats collects incremental statistics for a zone across multiple
 * frames. It is intentionally simple: it accumulates total time, tracks the
 * maximum observed sample, and counts samples so callers can compute averages.
 *
 * @note All times are expressed in milliseconds.
 */
struct ZoneStats {
    double totalMs = 0.0; ///< Total accumulated duration (ms)
    double maxMs   = 0.0; ///< Maximum single-sample duration (ms)
    size_t count   = 0;   ///< Number of samples observed

    /**
     * @brief Add a single duration sample to the statistics.
     * @param duration Duration of a zone sample in milliseconds.
     *
     * This method updates the running total, maximum, and increments the sample count.
     */
    void add(double duration) {
        totalMs += duration;
        if (duration > maxMs) maxMs = duration;
        ++count;
    }

    /**
     * @brief Compute the arithmetic mean (average) duration.
     * @return Average duration in milliseconds (0.0 if count == 0).
     */
    double avg() const { return count ? totalMs / static_cast<double>(count) : 0.0; }
};

/**
 * @class ProfilerUI
 * @brief Console-based ASCII profiler UI.
 *
 * The ProfilerUI class provides a simple way to visualize the results
 * produced by ChronoProfiler. It keeps a rolling history of the last
 * 'historySize' frames for context while also maintaining an absolute
 * 'totalFrames' counter for frame labeling.
 *
 * Typical usage:
 * @code
 * ProfilerUI profilerUI(60);          // keep last 60 frames
 * // in render loop, after ChronoProfiler::endFrame()
 * profilerUI.update();
 * profilerUI.render();
 * @endcode
 *
 * @par Thread-safety
 * Methods 'update()' and 'render()' are internally synchronized with
 * a 'std::mutex' ('uiMutex') so they may be called from different
 * threads as long as the calls themselves follow the profiler lifecycle
 * (i.e. update() after endFrame()).
 *
 * @par Design decisions
 * - Aggregated stats are cumulative (all-time). If you prefer sliding-window
 *   aggregation, change 'aggregatedStats' maintenance in 'update()'.
 * - ASCII bars are scaled by duration; consider dynamic scaling for large
 *   variance (enhancement suggestions below).
 *
 * @see ChronoProfiler
 */
class ProfilerUI {
public:
    /**
     * @brief Construct a ProfilerUI object.
     * @param historySize Maximum number of frames to retain in the rolling history (default 60).
     *
     * The history size bounds memory usage of the UI and controls how many
     * frames back the mini-history will allow you to inspect.
     */
    explicit ProfilerUI(size_t historySize = 60);

    /**
     * @brief Pull the latest frame events from ChronoProfiler and update internal state.
     *
     * This must be called after 'ChronoProfiler::endFrame()' (or when using RAII,
     * after the 'ScopedFrame' destructor runs) so that 'ChronoProfiler::getEvents()'
     * returns the merged events for the most recently completed frame.
     *
     * Responsibilities:
     *  - copy the merged frame event list into 'frameHistory'
     *  - maintain the 'maxHistory' rolling buffer by removing the oldest frame
     *    when capacity is exceeded
     *  - update 'aggregatedStats' for each zone encountered
     *  - increment the absolute 'totalFrames' counter used for frame labels
     *
     * Thread-safety: this function acquires 'uiMutex'.
     */
    void update();

     /**
     * @brief Render the current frame and aggregated statistics to stdout.
     *
     * The method prints:
     *  - a header line with the absolute frame number ('totalFrames')
     *  - an ASCII bar visualization of the most recent frame's zones
     *  - a table of aggregated statistics (Zone, Avg, Max, Count)
     *
     * @note 'render()' **forces output flushing** via 'std::flush'
     *       so UI updates appear immediately in interactive terminals.
     *       This avoids buffering delays, especially on platforms where
     *       stdout is line-buffered (no newline = no screen update).
     *
     * Thread-safety:
     *  - This function acquires 'uiMutex'.
     *  - Safe to call from a render or background thread.
     *
     * @see update()
     */
    void render();

private:
    size_t maxHistory; ///< Maximum frames retained in 'frameHistory'.

    /**
     * @brief Rolling history of frames.
     *
     * Each entry is a vector of 'ChronoProfiler::Event' representing events
     * captured for a single completed frame (merged from all threads).
     */
    std::vector<std::vector<ChronoProfiler::Event>> frameHistory;

    /**
     * @brief Aggregated all-time statistics for zones.
     *
     * Maps zone name -> ZoneStats. Zone names are copied into 'std::string'
     * keys for stable storage because ChronoProfiler::Event uses string_view.
     */
    std::unordered_map<std::string, ZoneStats> aggregatedStats;

    std::mutex uiMutex; ///< Protects 'update()' and 'render()'.

    /**
     * @brief Absolute count of frames rendered since creation.
     *
     * This is used for labeling frames in the UI and continues to grow
     * even when 'frameHistory' has reached 'maxHistory'.
     */
    size_t totalFrames = 0;

    /**
     * @brief Render a single frame's events as ASCII bars.
     * @param events Vector of events for the frame to render.
     * @param frameIndex Absolute index of the frame (for labeling).
     *
     * Each event is printed as:
     *   [Zone name padded] [ASCII bar proportional to duration] [N.NN ms] [ThreadName]
     *
     * Implementation notes:
     * - Bars are currently scaled linearly: 'barLength = int(durationMs * 10)'.
     * - Consider dynamic scaling or clamping for extremely large durations.
     */
    void renderFrame(const std::vector<ChronoProfiler::Event>& events, size_t frameIndex);

    /**
     * @brief Print aggregated statistics (Zone, Avg(ms), Max(ms), Count).
     *
     * The table uses 'std::setw' formatting to align columns. A caller may
     * prefer CSV or JSON output for automated post-processing; see
     * ChronoProfiler::exportToJSON() for JSON export of raw events.
     */
    void renderAggregatedStats();
};

#else // ======================= NO-OP VERSION ======================= //

/*
   Profiler disabled: UI becomes a no-op (zero overhead).

   Rationale: when profiling is disabled we want zero runtime overhead and no
   console spam. The no-op version preserves the class API so call sites can
   unconditionally create/use 'ProfilerUI' without littering the codebase
   with preprocessor checks.

   This mirrors the pattern used in ChronoProfiler.hpp: the real behavior is
   compiled under '#if defined(PROFILER)', while here we provide trivial
   implementations that compile away at optimization time. */
#include <cstddef>

/**
 * @class ProfilerUI
 * @brief No-op stub for builds without PROFILER enabled.
 *
 * All methods are trivial and do nothing. This allows engine code to call
 * 'profilerUI.update()' and 'profilerUI.render()' unconditionally.
 *
 * Example (works whether or not PROFILER is defined):
 * @code
 * ProfilerUI profilerUI;
 * // ... in loop ...
 * profilerUI.update();
 * profilerUI.render();
 * @endcode
 */
class ProfilerUI {
public:
    /**
     * @brief Construct a no-op ProfilerUI.
     * @param historySize Ignored.
     */
    explicit ProfilerUI(size_t = 60) {}

    /** @brief No-op update method. */
    void update() {}

    /** @brief No-op render method. */
    void render() {}
};

#endif // defined(PROFILER)
