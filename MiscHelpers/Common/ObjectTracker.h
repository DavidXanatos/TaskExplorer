#ifndef OBJECT_TRACKER_H
#define OBJECT_TRACKER_H

#include <string>
#include <map>
#include <atomic>
#include <mutex>

#include "../mischelpers_global.h"

class MISCHELPERS_EXPORT ObjectTrackerBase {
public:
    ObjectTrackerBase(const std::string& className);
    virtual ~ObjectTrackerBase();

    static void PrintCounts();

private:
    const std::string m_className;

    static std::map<std::string, std::atomic<int>>& getCounts();
    static std::mutex& getMutex();
};

#ifdef _DEBUG
#define TRACK_OBJECT(ClassName) ObjectTrackerBase objectTrackerInstance{ #ClassName };
#else
#define TRACK_OBJECT(ClassName)
#endif

#endif // OBJECT_TRACKER_H