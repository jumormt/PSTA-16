//
// Created by prophe cheng on 2025/1/17.
//

#ifndef PSTA_CONSEXESTATE_H
#define PSTA_CONSEXESTATE_H

#include "Bases/ExeState.h"
#include "Bases/SingleAbsValue.h"

#define NullptrID 0

namespace SVF
{

/*!
 * Constant Expr Execution State
 *
 * Constant expr execution state support z3 symbolic value
 * and gives a top value when two different constants join
 *
 * lattice:          ⊤                      may be constant
 *         /    /   |   \  \    \
 *       true ...  c0  c1 ...  false        constant
 *         \    \   \   |  |    |
 *                   ⊥                      not constant
 */
class ConsExeState final : public ExeState
{
    friend class SVFIR2ConsExeState;

public:
    typedef Map<u32_t, SingleAbsValue> VarToValMap;
    typedef VarToValMap LocToValMap;

    static ConsExeState* globalConsES;

public:
    ConsExeState(): ExeState(ExeState::SingleValueK) {}


    /// Constructor
    ConsExeState(VarToValMap varToValMap, LocToValMap locToValMap, Z3Expr brCond) : ExeState(ExeState::SingleValueK), _varToVal(
            SVFUtil::move(varToValMap)), _locToVal(SVFUtil::move(locToValMap)), _brCond(SVFUtil::move(brCond))  {}

    /// Copy Constructor
    ConsExeState(const ConsExeState &rhs) : ExeState(rhs), _varToVal(rhs.getVarToVal()),
                                            _locToVal(rhs.getLocToVal()), _brCond(rhs._brCond)
    {

    }

    /// Destructor
    ~ConsExeState() override
    {
        _varToVal.clear();
        _locToVal.clear();
    }

    /// Copy operator
    ConsExeState &operator=(const ConsExeState &rhs);

    /// Move Constructor
    ConsExeState(ConsExeState &&rhs) noexcept: ExeState(std::move(rhs)),
                                               _varToVal(SVFUtil::move(rhs._varToVal)), _locToVal(SVFUtil::move(rhs._locToVal)), _brCond(SVFUtil::move(rhs._brCond))
    {

    }

    /// Move operator
    ConsExeState &operator=(ConsExeState &&rhs) noexcept;

    /// Name
    static inline std::string name()
    {
        return "Constant Execution State";
    }

    using ExeState::operator==;
    using ExeState::operator!=;

    bool operator==(const ConsExeState &rhs) const;

    bool operator!=(const ConsExeState &other) const
    {
        return !(*this == other);
    }

    bool operator<(const ConsExeState &rhs) const;

    u32_t hash() const override;

protected:

    VarToValMap _varToVal; ///< Map a variable (symbol) to its constant value
    LocToValMap _locToVal; ///< Map a memory address to its stored constant value
    Z3Expr _brCond;

public:

    /// get memory addresses of variable
    virtual Addrs &getAddrs(u32_t id) override
    {
        assert(globalConsES && "global es not initialized?");
        auto it = globalConsES->_varToAddrs.find(id);
        if (it != globalConsES->_varToAddrs.end()) return it->second;
        return _varToAddrs[id];
    }

    /// get constant value of variable
    inline SingleAbsValue &operator[](u32_t varId)
    {
        assert(globalConsES && "global es not initialized?");
        auto it = globalConsES->_varToVal.find(varId);
        if (it != globalConsES->_varToVal.end())
            return it->second;
        else
            return _varToVal[varId];
    }

    /// whether the variable is in varToAddrs table
    virtual inline bool inVarToAddrsTable(u32_t id) const override
    {
        assert(globalConsES && "global es not initialized?");
        return _varToAddrs.find(id) != _varToAddrs.end() ||
               globalConsES->_varToAddrs.find(id) != globalConsES->_varToAddrs.end();
    }

    /// whether the variable is in varToVal table
    inline bool inVarToValTable(u32_t varId) const
    {
        assert(globalConsES && "global es not initialized?");
        return _varToVal.count(varId) || globalConsES->_varToVal.count(varId);
    }

    /// whether the memory address stores memory addresses
    virtual inline bool inLocToAddrsTable(u32_t id) const override
    {
        assert(globalConsES && "global es not initialized?");
        return globalConsES->_locToAddrs.find(id) != globalConsES->_locToAddrs.end() || inLocalLocToAddrsTable(id);
    }

    /// whether the memory address stores constant value
    inline bool inLocToValTable(u32_t varId) const
    {
        assert(globalConsES && "global es not initialized?");
        return inLocalLocToValTable(varId) || globalConsES->_locToVal.count(varId);
    }

    inline const VarToValMap &getVarToVal() const
    {
        return _varToVal;
    }

    inline const LocToValMap &getLocToVal() const
    {
        return _locToVal;
    }

    virtual inline bool inLocalLocToAddrsTable(u32_t id) const
    {
        return _locToAddrs.find(id) != _locToAddrs.end();
    }

    inline bool inLocalLocToValTable(u32_t varId) const
    {
        return _locToVal.count(varId);
    }

    inline bool inLocalLocToValTable(const SingleAbsValue& addr) const
    {
        return _locToVal.count(getInternalID(addr.getNumeral()));
    }

public:

    /// Merge rhs into this
    bool joinWith(const ConsExeState &rhs);

    /// Build global execution state
    void buildGlobES(ConsExeState &globES, Set<u32_t> &vars);

    /// Update symbolic states based on the summary/side-effect of callee
    void applySummary(const ConsExeState &summary);

    static inline z3::context &getContext() {
        return Z3Expr::getContext();
    }

    /// Get z3 context, singleton design here to make sure we only have one context
    static inline z3::solver &getSolver() {
        return Z3Expr::getSolver();
    }

    static inline z3::check_result solverCheck(const Z3Expr &e) {
        Z3Expr::getSolver().push();
        Z3Expr::getSolver().add(e.getExpr());
        z3::check_result res = Z3Expr::getSolver().check();
        Z3Expr::getSolver().pop();
        return res;
    }

    /// Whether path constraint is satisfiable
    inline bool isSatisfiable() const {
        return solverCheck(_brCond) != z3::unsat;
    }

    inline bool equalVar(u32_t lhs, u32_t rhs)
    {
        if (!inVarToValTable(lhs) || !inVarToValTable(rhs)) return false;
        return eq((*this)[lhs], (*this)[rhs]);
    }

    /// Whether state is null state (uninitialized state)
    inline bool isNullState() const
    {
        return _varToVal.size() == 1 && eq((*_varToVal.begin()).second, -1) && _locToVal.empty() && eq(_brCond, Z3Expr::nullExpr());
    }

    /// Print values of all expressions
    void printExprValues() const;

    /// Print values of all expressions
    void printExprValues(std::ostream &oss) const override;

    std::string toString() const override;

    std::string pcToString() const;

    std::string varToString(u32_t varId) const;

    std::string locToString(u32_t objId) const;

    bool applySelect(u32_t res, u32_t cond, u32_t top, u32_t fop);

    bool applyPhi(u32_t res, std::vector<u32_t> &ops);

    virtual Addrs &loadAddrs(u32_t addr) override
    {
        assert(globalConsES && "global es not initialized?");
        assert(isVirtualMemAddress(addr) && "not virtual address?");
        u32_t objId = getInternalID(addr);
        auto it = _locToAddrs.find(objId);
        if (it != _locToAddrs.end())
        {
            return it->second;
        }
        else
        {
            auto globIt = globalConsES->_locToAddrs.find(objId);
            if (globIt != globalConsES->_locToAddrs.end())
            {
                return globIt->second;
            }
            else
            {
                return getAddrs(0);
            }
        }
    }

    virtual std::string varToAddrs(u32_t varId) const override
    {
        assert(globalConsES && "global es not initialized?");
        std::stringstream exprName;
        auto it = _varToAddrs.find(varId);
        if (it == _varToAddrs.end())
        {
            auto git = globalConsES->_varToAddrs.find(varId);
            if (git == globalConsES->_varToAddrs.end())
                exprName << "Var not in varToAddrs!\n";
            else
            {
                const Addrs &vaddrs = git->second;
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

    virtual std::string locToAddrs(u32_t objId) const override
    {
        assert(globalConsES && "global es not initialized?");
        std::stringstream exprName;
        auto it = _locToAddrs.find(objId);
        if (it == _locToAddrs.end())
        {
            auto git = globalConsES->_locToAddrs.find(objId);
            if (git == globalConsES->_locToAddrs.end())
                exprName << "Obj not in locToVal!\n";
            else
            {
                const Addrs &vaddrs = git->second;
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

    /// Empty execution state with a true path constraint
    static inline ConsExeState initExeState()
    {
        VarToValMap mp;
        ConsExeState exeState(mp, SVFUtil::move(mp), Z3Expr::getTrueCond());
        return SVFUtil::move(exeState);
    }

    /// Empty execution state with a null expr
    static inline ConsExeState nullExeState()
    {
        VarToValMap mp;
        ConsExeState exeState(mp, SVFUtil::move(mp), Z3Expr::nullExpr());
        exeState._varToVal[NullptrID] = -1;
        return SVFUtil::move(exeState);
    }

    inline const Z3Expr &getBrCond() const
    {
        return _brCond;
    }

    inline void setBrCond(const Z3Expr& brCond) {
        _brCond = brCond.simplify();
    }

public:



    s64_t getNumber(u32_t lhs);

    static inline SingleAbsValue getIntOneZ3Expr()
    {
        return getContext().int_val(1);
    }

    static inline SingleAbsValue getIntZeroZ3Expr()
    {
        return getContext().int_val(0);
    }

    static inline SingleAbsValue getTrueZ3Expr()
    {
        return getContext().bool_val(true);
    }

    static inline SingleAbsValue getFalseZ3Expr()
    {
        return getContext().bool_val(false);
    }

public:

    /// Store value to location
    bool store(const SingleAbsValue &loc, const SingleAbsValue &value);

    /// Load value at location
    SingleAbsValue load(const SingleAbsValue &loc);

    /// Return int value from an expression if it is a numeral, otherwise return an approximate value
    static inline s32_t z3Expr2NumValue(const SingleAbsValue &e)
    {
        assert(e.is_numeral() && "not numeral?");
        int64_t i;
        if(e.getExpr().is_numeral_i64(i))
            return e.get_numeral_int64();
        else
        {
            return e.leq(0) ? INT32_MIN : INT32_MAX;
        }
    }

    /// Whether two var to value map is equivalent
    static bool eqVarToValMap(const VarToValMap &pre, const VarToValMap &nxt);


    /// Whether lhs is less than rhs
    static bool lessThanVarToValMap(const VarToValMap &lhs, const VarToValMap &rhs);


private:


    static bool assign(SingleAbsValue &lhs, const SingleAbsValue &rhs);

    inline bool store(u32_t objId, const SingleAbsValue &z3Expr)
    {
        SingleAbsValue &lhs = _locToVal[objId];
        if (!eq(lhs, z3Expr.simplify()))
        {
            lhs = z3Expr.simplify();
            return true;
        }
        else
            return false;
    }

    inline SingleAbsValue load(u32_t objId);
}; // end class ConExeState

} // end namespace SVF

/// Specialized hash for ConExeState
template<>
struct std::hash<SVF::ConsExeState>
{
    size_t operator()(const SVF::ConsExeState &exeState) const
    {
        return exeState.hash();
    }
};

#endif //PSTA_CONSEXESTATE_H
