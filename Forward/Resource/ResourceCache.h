#pragma once

#include "../Common/stdafx.h"
#include <map>
#include <memory>
#include <cwctype>

inline std::wstring resource_key(const std::filesystem::path& path)
{
    auto key = std::filesystem::weakly_canonical(std::filesystem::absolute(path)).generic_wstring();
    std::transform(key.begin(), key.end(), key.begin(),
        [](wchar_t value) { return static_cast<wchar_t>(std::towlower(value)); });
    return key;
}

template<class T>
class RESOURCE_CACHE
{
public:
    std::shared_ptr<T> find(const std::wstring& key) const
    {
        const auto found = _entries.find(key);
        return found == _entries.end() ? nullptr : found->second.lock();
    }
    void insert(const std::wstring& key, const std::shared_ptr<T>& resource) { _entries[key] = resource; }
    void prune()
    {
        std::erase_if(_entries, [](const auto& entry) { return entry.second.expired(); });
    }
    size_t live_count() const
    {
        return std::count_if(_entries.begin(), _entries.end(),
            [](const auto& entry) { return !entry.second.expired(); });
    }
private:
    std::map<std::wstring, std::weak_ptr<T>> _entries;
};
