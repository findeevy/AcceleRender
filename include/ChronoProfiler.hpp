#pragma once

/* OPTIONAL PROFILER WRAPPER
   Enable profiler by defining -DPROFILER in your build flags.
   If NOT defined, everything becomes zero-cost no-op stubs. */
#if defined(PROFILER)

// ---------------- REAL PROFILER IMPLEMENTATION ---------------- //

/**
 * @file ChronoProfiler.hpp
 * @brief Header for a lightweight, zone-based CPU profiler for real-time applications.
 *
 * This profiler collects per-frame, per-thread timing data for code regions
 * instrumented with PROFILE_SCOPE(name). It is designed for minimal overhead and
 * thread-safe operation in multi-threaded engines (graphics/game/simulation).
 *
 * @note Optional features include thread naming, zone colors/categories, ring-buffer
 * limits, and JSON export for offline analysis.
 */

#include <chrono>        ///< High-resolution timers (std::chrono::high_resolution_clock)
#include <string>        ///< std::string used for thread names and zone categories
#include <string_view>   ///< std::string_view for lightweight zone names
#include <vector>        ///< Dynamic arrays used for storing profiling events
#include <thread>        ///< std::this_thread::get_id to identify threads
#include <mutex>         ///< std::mutex to safely merge thread-local data
#include <unordered_map> ///< Map thread IDs to human-readable thread names
#include <atomic>        ///< Atomic counters for defensive tracking of event counts
#include <cstdint>

/**
 * @class ChronoProfiler
 * @brief A real-time CPU profiler with per-thread, zone-based event recording.
 *
 * Use PROFILE_SCOPE("name") to mark any block of code for timing. The profiler
 * stores events in thread-local buffers during the frame, then merges them at
 * endFrame() for visualization or export.
 *
 * Optional enhancements:
 *  - Thread names for clearer timeline display
 *  - Zone colors/categories for visualization grouping
 *  - Preallocated ring buffers to limit memory usage
 *  - JSON export for offline profiling sessions
 */
class ChronoProfiler {
public:
    /**
     * @struct Event
     * @brief Stores timing information for a single code zone.
     *
     * This struct is created when a zone begins (startMs) and completed when
     * it ends (durationMs). It contains optional metadata for visualization.
     */
    struct Event {
        std::string_view name; ///< Zone name (caller-owned string literal preferred)
        double startMs;        ///< Timestamp relative to frame start
        double durationMs;     ///< Duration of the zone in milliseconds
        uint32_t threadId;     ///< Numeric ID representing the thread

        // Optional visualization metadata
        uint32_t color;        ///< RGBA color for UI display of this zone
        std::string category;  ///< Optional grouping/category for zones
    };

    /**
     * @brief Starts a new profiling frame.
     *
     * Clears the previous frame's merged events and stores the reference start time.
     * Call this once per frame, typically at the beginning of your render/update loop.
     */
    static void beginFrame();

    /**
     * @brief Ends the current profiling frame.
     *
     * Merges all thread-local events from all threads into a global frameEvents vector.
     * Only here is a lock required (mergeMutex), ensuring minimal overhead during
     * normal profiling.
     */
    static void endFrame();

    /**
     * @brief Marks the start of a profiling zone.
     *
     * Normally called internally via ScopedZone/PROFILE_SCOPE. Records start
     * timestamp, thread ID, and optional color/category.
     *
     * @param name Zone name
     * @param color Optional RGBA color (default: cyan-ish)
     * @param category Optional category string for visualization grouping
     */
    static void pushEventStart(std::string_view name, uint32_t color = 0x64C8FFFF, const std::string& category = "");

    /**
     * @brief Ends the most recent profiling zone on this thread.
     *
     * Calculates duration, offsets startMs relative to frameStart, and finalizes the event.
     * Does nothing if there is no corresponding start (defensive check).
     */
    static void pushEventEnd();

    /**
     * @brief Returns merged events for the last completed frame.
     *
     * @return Reference to vector of Events. Do not store long-term!
     */
    static const std::vector<Event>& getEvents();

    /**
     * @brief Retrieves a human-readable name for a thread ID.
     *
     * @param threadId Numeric thread ID
     * @return String name if registered, else "<unnamed>"
     */
    static std::string getThreadName(uint32_t threadId);

    /**
     * @brief Assigns a human-readable name to the calling thread.
     *
     * Useful for labeling timeline tracks in visualization.
     *
     * @param name Thread name string
     */
    static void setThreadName(const std::string& name);

    /**
     * @brief Exports the current profiling session to a JSON file.
     *
     * Can be used for offline analysis or saving frame history.
     *
     * @param filename Path to output JSON file
     */
    static void exportToJSON(const std::string& filename);

    /**
     * @class ScopedZone
     * @brief Automatically begins and ends a profiling zone using RAII.
     *
     * Use PROFILE_SCOPE("ZoneName") to instrument code.
     */
    class ScopedZone {
    public:
        explicit ScopedZone(std::string_view name, uint32_t color = 0x64C8FFFF, const std::string& category = "") {
            ChronoProfiler::pushEventStart(name, color, category);
        }
        ~ScopedZone() { ChronoProfiler::pushEventEnd(); }
    };

    /**
     * @class ScopedFrame
     * @brief RAII helper for profiling an entire frame.
     *
     * Constructs a frame profiler that automatically calls ChronoProfiler::beginFrame()
     * on construction, and ChronoProfiler::endFrame() when it destructs.
     *
     * Usage:
     * @code
     * void mainLoop() {
     *     while (!glfwWindowShouldClose(window)) {
     *         glfwPollEvents();
     *
     *         ScopedFrame frame;          // automatically begins frame
     *         PROFILE_SCOPE("drawFrame"); // zone profiling inside frame
     *         drawFrame();
     *     } // frame destructor automatically ends the frame
     * }
     * @endcode
     */
    class ScopedFrame {
    public:
        /**
         * @brief Start a new profiling frame.
         */
        ScopedFrame() { ChronoProfiler::beginFrame(); }

        /**
         * @brief Finalize profiling frame on scope exit.
         */
        ~ScopedFrame() { ChronoProfiler::endFrame(); }
    };

private:

    /** @brief Thread-local event list for each thread. */
    static thread_local std::vector<Event> threadEvents;

    static constexpr size_t kMaxEventsPerThread = 1024; ///< Optional ring buffer limit

    /** @brief Final merged events for the current frame. */
    static std::vector<Event> frameEvents;

    /** @brief Mapping from thread IDs to human-readable names. */
    static std::unordered_map<uint32_t, std::string> threadNames;

    /** @brief Mutex protecting access to threadNames map. */
    static std::mutex threadNamesMutex;

    /** @brief Mutex protecting merging of thread-local events into frameEvents. */
    static std::mutex mergeMutex;

    /** @brief Frame start timestamp. */
    static std::chrono::high_resolution_clock::time_point frameStart;

    /** @brief Returns high-resolution current time in milliseconds. */
    static double nowMs();

    /** @brief Total event counter to prevent runaway event generation. */
    static std::atomic<size_t> totalEventCount;

    /** @brief Stores all thread-local buffers for multi-threaded merging. */
    static std::vector<std::vector<Event>*> allThreadBuffers;
};

/**
 * @def PROFILE_SCOPE(name)
 * @brief Profiles a scope automatically using RAII.
 *
 * Usage:
 * @code
 * PROFILE_SCOPE("Physics Update");
 * @endcode
 */
#define PROFILE_SCOPE(name) ChronoProfiler::ScopedZone _scope_##__LINE__(name)

// ---------------- END REAL PROFILER IMPLEMENTATION ---------------- //

#else


/* FAKE / NO-OP PROFILER IMPLEMENTATION (when PROFILER is NOT defined)
   This version satisfies the compiler and ensures that ALL profiler calls
   (ChronoProfiler::ScopedFrame, ScopedZone, PROFILE_SCOPE) compile with
   *zero overhead* and without modifying any call sites.

   + Requires NO code changes anywhere else in the engine
   + Eliminates all profiling logic at compile time
   + Ensures symbols still exist so linking never breaks */

#include <vector>
#include <string>
#include <string_view>
#include <cstdint>

/**
 * @class ChronoProfiler
 * @brief No-op stub implementation used when profiling is disabled.
 *
 * When 'PROFILER' is **not** defined at compile time, the profiler becomes a
 * completely empty system — every API function resolves to a no-op.
 *
 * This enables engine code to freely use:
 * - 'ChronoProfiler::ScopedFrame'
 * - 'ChronoProfiler::ScopedZone'
 * - 'PROFILE_SCOPE("...")'
 *
 * without needing preprocessor guards like '#if PROFILER' anywhere else.
 *
 * ### Compile-time behavior
 * - All methods are 'inline' trivial or empty
 * - No allocations, no timers, no string copies
 * - Returned collections are empty references (safe)
 *
 * @note This prevents any runtime overhead when profiling is disabled.
 */
class ChronoProfiler {
public:
    /**
     * @struct Event
     * @brief Dummy struct placeholder so return types remain valid.
     *
     * Even though no events are ever stored, callers may still expect a type
     * named 'Event' and a 'std::vector<Event>' return type.
     */
    struct Event {
        std::string name = "";
        uint32_t threadId = 0;
        double durationMs = 0.0;
    };

    /**
     * @brief Begin a new profiling frame.
     *
     * Does nothing. Exists solely to keep calling code identical between
     * profiler-enabled and no-op builds.
     */
    static void beginFrame() {}

    /**
     * @brief End the current profiling frame.
     *
     * Does nothing. Exists solely for API symmetry.
     */
    static void endFrame() {}

    /**
     * @brief Begin recording a profiling zone.
     *
     * @param name Name of the profiling scope (ignored)
     * @param color Suggested UI color if visualization exists (ignored)
     * @param category Optional category grouping (ignored)
     */
    static void pushEventStart(std::string_view /*name*/, uint32_t /*color*/ = 0, const std::string& /*category*/ = "") {}

    /**
     * @brief End the most recently recorded profiling zone.
     *
     * No stack tracking, no timings, no dependencies.
     */
    static void pushEventEnd() {}

    /**
     * @brief Return the list of recorded events (always empty).
     *
     * @return const std::vector<Event>& Reference to a static empty vector.
     */
    static const std::vector<Event>& getEvents() {
        static std::vector<Event> empty; ///< Local static avoids global initialization
        return empty;
    }

    /**
     * @brief Retrieve thread name (always empty string).
     *
     * @param threadId Ignored
     * @return std::string Always empty
     */
    static std::string getThreadName(uint32_t /*threadId*/) {
        return {};
    }

    /**
     * @brief Assign a name to the calling thread (ignored).
     *
     * @param name Human-readable thread name
     */
    static void setThreadName(const std::string& /*name*/) {}

    /**
     * @brief Export profiling data to JSON (ignored).
     *
     * @param filename Filename to write JSON to (ignored)
     */
    static void exportToJSON(const std::string& /*filename*/) {}

    /**
     * @class ScopedZone
     * @brief No-op RAII wrapper matching real profiler signature.
     *
     * Constructing/destroying a ScopedZone in this configuration generates
     * absolutely no instructions (the compiler removes all calls).
     */
    class ScopedZone {
    public:
        /**
         * @brief Construct a profiling zone (ignored).
         *
         * @param name Name of the profiling zone (ignored)
         * @param color UI color for visualizers (ignored)
         * @param category Optional grouping tag (ignored)
         */
        ScopedZone(std::string_view /*name*/, uint32_t /*color*/ = 0, const std::string& /*category*/ = "") {}
    };

    /**
     * @class ScopedFrame
     * @brief No-op RAII object matching real profiler.
     *
     * Exists so call sites can declare 'ScopedFrame frame;' unconditionally.
     */
    class ScopedFrame {
    public:
        /** @brief Begin frame (ignored). */
        ScopedFrame() {}

        /** @brief End frame (ignored). */
        ~ScopedFrame() {}
    };
};

/**
 * @def PROFILE_SCOPE(name)
 * @brief Macro expands to nothing when profiler is disabled.
 *
 * Usage (remains valid in all builds):
 * @code
 * PROFILE_SCOPE("UpdatePhysics");
 * @endcode
 */
#define PROFILE_SCOPE(name)

// end of file
#endif
