#ifndef __bf_elapsed_timer_h__
#define __bf_elapsed_timer_h__

#include <string>
#include <chrono>
#include <vector>
#include <sstream>
#include <stdio.h>

struct BfElapsedRecord
{
    std::string name;
    std::chrono::steady_clock::time_point start;
    std::chrono::steady_clock::time_point end;
};

struct BfElapsedTimer
{
    void start(const char *name)
    {
        mRecords.push_back({name, std::chrono::steady_clock::now(), std::chrono::steady_clock::time_point()});
    }

    void stop()
    {
        if (mRecords.empty())
        {
            throw std::runtime_error("No timer started");
        }
        mRecords.back().end = std::chrono::steady_clock::now();
    }

    static std::string nanosecondToString(uint64_t ns)
    {
        const char *units[] = {"us", "ms", "sec"};

        if (ns < 1000)
        {
            return std::to_string(ns) + " ns";
        }

        double val = static_cast<double>(ns) / 1000.0;
        int idx = 0;

        while (val >= 1000.0 && idx < 2)
        {
            val /= 1000.0;
            idx++;
        }
        std::ostringstream oss;
        oss.precision(2);
        oss << std::fixed << val << " " << units[idx];
        return oss.str();
    }


    void printElapsedTimes()
    {
        for (const auto &record : mRecords)
        {
            uint64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(record.end - record.start).count();
            std::string elapsedTime = nanosecondToString(ns);
            fprintf(stderr, "%10s: %s\n", record.name.c_str(), elapsedTime.c_str());
        }
    }

    std::vector<BfElapsedRecord> mRecords;
};

#endif
