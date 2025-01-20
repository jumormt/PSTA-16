//
// Created by prophe cheng on 2025/1/17.
//

#include "Bases/ExeState.h"

using namespace SVF;

bool ExeState::operator==(const ExeState &rhs) const
{
    return eqVarToAddrs(_varToAddrs, rhs._varToAddrs) && eqVarToAddrs(_locToAddrs, rhs._locToAddrs);
}

bool ExeState::joinWith(const ExeState &other)
{
    bool changed = false;
    for (auto it = other._varToAddrs.begin(); it != other._varToAddrs.end(); ++it)
    {
        auto key = it->first;
        auto oit = _varToAddrs.find(key);
        if (oit != _varToAddrs.end())
        {
            if(oit->second.join_with(it->second))
                changed = true;
        }
        else
        {
            changed = true;
            _varToAddrs.emplace(key, it->second);
        }
    }
    for (auto it = other._locToAddrs.begin(); it != other._locToAddrs.end(); ++it)
    {
        auto key = it->first;
        auto oit = _locToAddrs.find(key);
        if (oit != _locToAddrs.end())
        {
            if(oit->second.join_with(it->second))
                changed = true;
        }
        else
        {
            changed = true;
            _locToAddrs.emplace(key, it->second);
        }
    }
    return changed;
}

bool ExeState::meetWith(const ExeState &other)
{
    bool changed = false;
    for (auto it = other._varToAddrs.begin(); it != other._varToAddrs.end(); ++it)
    {
        auto key = it->first;
        auto oit = _varToAddrs.find(key);
        if (oit != _varToAddrs.end())
        {
            if(oit->second.meet_with(it->second))
                changed = true;
        }
    }
    for (auto it = other._locToAddrs.begin(); it != other._locToAddrs.end(); ++it)
    {
        auto key = it->first;
        auto oit = _locToAddrs.find(key);
        if (oit != _locToAddrs.end())
        {
            if(oit->second.meet_with(it->second))
                changed = true;
        }
    }
    return changed;
}

u32_t ExeState::hash() const
{
    size_t h = getVarToAddrs().size() * 2;
    Hash<u32_t> hf;
    for (const auto &t: getVarToAddrs())
    {
        h ^= hf(t.first) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    size_t h2 = getLocToAddrs().size() * 2;
    for (const auto &t: getLocToAddrs())
    {
        h2 ^= hf(t.first) + 0x9e3779b9 + (h2 << 6) + (h2 >> 2);
    }
    Hash<std::pair<u32_t, u32_t>> pairH;
    return pairH(std::make_pair(h, h2));
}
