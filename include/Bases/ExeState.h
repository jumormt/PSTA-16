//
// Created by prophe cheng on 2025/1/17.
//

#ifndef PSTA_EXESTATE_H
#define PSTA_EXESTATE_H

#include "Bases/NumericLiteral.h"
#include "Bases/AddressValue.h"
#include "Util/Z3Expr.h"
#include "Util/SVFUtil.h"

namespace SVF {

class ExeState
{
    friend class SVFIR2ItvExeState;

public:

    typedef AddressValue Addrs;
    typedef Map<u32_t, Addrs> VarToAddrs;
    /// Execution state type
    enum ExeState_TYPE
    {
        IntervalK, SingleValueK
    };
private:
    ExeState_TYPE _kind;

public:
    ExeState(ExeState_TYPE kind) : _kind(kind) {}

    virtual ~ExeState() = default;

    ExeState(const ExeState &rhs) : _varToAddrs(rhs._varToAddrs),
                                    _locToAddrs(rhs._locToAddrs) {}

    ExeState(ExeState &&rhs) noexcept: _varToAddrs(std::move(rhs._varToAddrs)),
                                       _locToAddrs(std::move(rhs._locToAddrs)) {}

    ExeState &operator=(const ExeState &rhs)
    {
        if(*this != rhs)
        {
            _varToAddrs = rhs._varToAddrs;
            _locToAddrs = rhs._locToAddrs;
        }
        return *this;
    }

    ExeState &operator=(ExeState &&rhs) noexcept
    {
        if (this != &rhs)
        {
            _varToAddrs = std::move(rhs._varToAddrs);
            _locToAddrs = std::move(rhs._locToAddrs);
        }
        return *this;
    }


protected:
    VarToAddrs _varToAddrs{{0, getVirtualMemAddress(0)}}; ///< Map a variable (symbol) to its memory addresses
    VarToAddrs _locToAddrs;                               ///< Map a memory address to its stored memory addresses

public:

    /// get memory addresses of variable
    virtual Addrs &getAddrs(u32_t id)
    {
        return _varToAddrs[id];
    }

    /// whether the variable is in varToAddrs table
    inline virtual bool inVarToAddrsTable(u32_t id) const
    {
        return _varToAddrs.find(id) != _varToAddrs.end();
    }

    /// whether the memory address stores memory addresses
    inline virtual bool inLocToAddrsTable(u32_t id) const
    {
        return _locToAddrs.find(id) != _locToAddrs.end();
    }


    inline virtual const VarToAddrs &getVarToAddrs() const
    {
        return _varToAddrs;
    }

    inline virtual const VarToAddrs &getLocToAddrs() const
    {
        return _locToAddrs;
    }

public:
    /// Make all value join with the other
    bool joinWith(const ExeState &other);

    /// Make all value meet with the other
    bool meetWith(const ExeState &other);

    virtual u32_t hash() const;

    /// Print values of all expressions
    virtual void printExprValues(std::ostream &oss) const {}

    virtual std::string toString() const
    {
        return "";
    }

    inline ExeState_TYPE getExeStateKind() const
    {
        return _kind;
    }



public:
    inline virtual void storeAddrs(u32_t addr, const Addrs &vaddrs)
    {
        assert(isVirtualMemAddress(addr) && "not virtual address?");
        if(isNullPtr(addr)) return;
        u32_t objId = getInternalID(addr);
        _locToAddrs[objId] = vaddrs;
    }

    inline virtual Addrs &loadAddrs(u32_t addr)
    {
        assert(isVirtualMemAddress(addr) && "not virtual address?");
        u32_t objId = getInternalID(addr);
        return _locToAddrs[objId];
    }

    inline bool isNullPtr(u32_t addr)
    {
        return getInternalID(addr) == 0;
    }

public:

    virtual bool operator==(const ExeState &rhs) const;

    inline virtual bool operator!=(const ExeState &rhs) const
    {
        return !(*this == rhs);
    }

    bool equals(const ExeState *other) const
    {
        return false;
    }

protected:

    static bool eqVarToAddrs(const VarToAddrs &lhs, const VarToAddrs &rhs)
    {
        if (lhs.size() != rhs.size()) return false;
        for (const auto &item: lhs)
        {
            auto it = rhs.find(item.first);
            if (it == rhs.end())
                return false;
            if (item.second != it->second)
            {
                return false;
            }
        }
        return true;
    }

public:

    virtual std::string varToAddrs(u32_t varId) const
    {
        std::stringstream exprName;
        auto it = _varToAddrs.find(varId);
        if (it == _varToAddrs.end())
        {
            exprName << "Var not in varToAddrs!\n";
        }
        else
        {
            const Addrs &vaddrs = it->second;
            if (vaddrs.size() == 1)
            {
                exprName << "addr: {" << std::dec << getInternalID(*vaddrs.begin()) << "}\n";
            }
            else
            {
                exprName << "addr: {";
                for (const auto &addr: vaddrs)
                {
                    exprName << std::dec << getInternalID(addr) << ", ";
                }
                exprName << "}\n";
            }
        }
        return SVFUtil::move(exprName.str());
    }

    virtual std::string locToAddrs(u32_t objId) const
    {
        std::stringstream exprName;
        auto it = _locToAddrs.find(objId);
        if (it == _locToAddrs.end())
        {
            exprName << "Var not in varToAddrs!\n";
        }
        else
        {
            const Addrs &vaddrs = it->second;
            if (vaddrs.size() == 1)
            {
                exprName << "addr: {" << std::dec << getInternalID(*vaddrs.begin()) << "}\n";
            }
            else
            {
                exprName << "addr: {";
                for (const auto &addr: vaddrs)
                {
                    exprName << std::dec << getInternalID(addr) << ", ";
                }
                exprName << "}\n";
            }
        }
        return SVFUtil::move(exprName.str());
    }

public:
    static z3::context &getContext()
    {
        return Z3Expr::getContext();
    }

    /// The physical address starts with 0x7f...... + idx
    static inline u32_t getVirtualMemAddress(u32_t idx)
    {
        return AddressValue::getVirtualMemAddress(idx);
    }

    /// Check bit value of val start with 0x7F000000, filter by 0xFF000000
    static inline bool isVirtualMemAddress(u32_t val)
    {
        return AddressValue::isVirtualMemAddress(val);
    }

    /// Return the internal index if idx is an address otherwise return the value of idx
    static inline u32_t getInternalID(u32_t idx)
    {
        return AddressValue::getInternalID(idx);
    }
}; // end class ExeState
}

template<>
struct std::hash<SVF::ExeState>
{
    size_t operator()(const SVF::ExeState &es) const
    {
        return es.hash();
    }
};

#endif //PSTA_EXESTATE_H
