#include "stdafx.h"
#include "ObjectTracker.h"
#include "DebugHelpers.h"

ObjectTrackerBase::ObjectTrackerBase(const std::string& className)
    : m_className(className)
{
    std::atomic<int>* counter = nullptr;
    {
        std::lock_guard<std::mutex> lock(getMutex());
        auto& counts = getCounts();
        auto it = counts.find(m_className);
        if (it == counts.end()) {
            it = counts.emplace(m_className, 0).first;
        }
        counter = &it->second;
    }
    counter->fetch_add(1, std::memory_order_relaxed);
}

ObjectTrackerBase::~ObjectTrackerBase()
{
    std::atomic<int>* counter = nullptr;
    {
        std::lock_guard<std::mutex> lock(getMutex());
        auto& counts = getCounts();
        auto it = counts.find(m_className);
        if (it != counts.end()) {
            counter = &it->second;
        }
    }
    if (counter) {
        counter->fetch_sub(1, std::memory_order_relaxed);
    }
}

std::map<std::string, std::atomic<int>>& ObjectTrackerBase::getCounts()
{
    static std::map<std::string, std::atomic<int>> counts;
    return counts;
}

std::mutex& ObjectTrackerBase::getMutex()
{
    static std::mutex mtx;
    return mtx;
}

void ObjectTrackerBase::PrintCounts()
{
	DbgPrint(L"Object counts:\n");
	DbgPrint(L"---\n");
    std::lock_guard<std::mutex> lock(getMutex());
    for (const auto& pair : getCounts()) {
        int count = pair.second.load(std::memory_order_relaxed);
        if (count != 0) {
			DbgPrint(L"%S: %d\n", pair.first.c_str(), count);
        }
    }
	DbgPrint(L"+++\n");
}