//
// Created by prophe cheng on 2025/1/17.
//

#ifndef PSTA_SYMSTATE_H
#define PSTA_SYMSTATE_H

#include "Bases/ConsExeState.h"

namespace SVF
{
enum class TypeState {
    Uinit = 0,
    Error,
    Allocated,
    Freed,
    Opened,
    Unknown
};

class TypeStateParser {
public:
    static Map<std::string, TypeState> _typeMap;
    static Map<TypeState, std::string> _revTypeMap;

    static TypeState fromString(const std::string &str) {
        auto it = _typeMap.find(str);
        if (it == _typeMap.end())
            return TypeState::Unknown;
        else
            return it->second;
    }

    static std::string toString(const TypeState& typeState) {
        auto it = _revTypeMap.find(typeState);
        if (it == _revTypeMap.end())
            return "Unknown";
        else
            return it->second;
    }
};

/*!
 * Symbolic state
 *
 * Execution State + Type State
 */
class SymState
{

public:
    typedef std::vector<u32_t> KeyNodes;
    typedef Set<KeyNodes> KeyNodesSet;

private:
    ConsExeState _exeState; ///< Execution state: values of variables
    TypeState _typeState; ///< Type state: FSM node

private:
    /// Only for bug report
    KeyNodesSet _keyNodesSet; ///< The nodes where abstract state changes
    Z3Expr _branchCondition; ///< The branches current state passes

public:
    /// Constructor
    SymState() : _exeState(ConsExeState::nullExeState()), _typeState(TypeState::Unknown) {}

    /// Constructor
    SymState(ConsExeState _es, TypeState _as);

    /// Destructor
    virtual ~SymState() = default;

    /// Copy Constructor
    SymState(const SymState &rhs) : _exeState(rhs._exeState), _typeState(rhs._typeState), _keyNodesSet(rhs._keyNodesSet),
                                    _branchCondition(rhs._branchCondition)
    {

    }

    /// Operator=
    SymState &operator=(const SymState &rhs)
    {
        if (*this != rhs)
        {
            _typeState = rhs._typeState;
            _exeState = rhs._exeState;
            _keyNodesSet = rhs._keyNodesSet;
            _branchCondition = rhs._branchCondition;
        }
        return *this;
    }


    /// Move Constructor
    SymState(SymState &&rhs) noexcept: _exeState(SVFUtil::move(rhs._exeState)),
                                       _typeState(SVFUtil::move(rhs._typeState)),
                                       _keyNodesSet(SVFUtil::move(rhs._keyNodesSet)),
                                       _branchCondition(rhs._branchCondition)
    {

    }

    /// Move operator=
    SymState &operator=(SymState &&rhs) noexcept
    {
        if (this != &rhs)
        {
            _typeState = SVFUtil::move(rhs._typeState);
            _exeState = SVFUtil::move(rhs._exeState);
            _keyNodesSet = SVFUtil::move(rhs._keyNodesSet);
            _branchCondition = rhs._branchCondition;
        }
        return *this;
    }

    const KeyNodesSet &getKeyNodesSet() const
    {
        return _keyNodesSet;
    }


    void insertKeyNode(NodeID id)
    {
        if (_keyNodesSet.empty())
        {
            _keyNodesSet.insert(KeyNodes{id});
        }
        else
        {
            for (const auto &df: _keyNodesSet)
            {
                const_cast<KeyNodes &>(df).push_back(id);
            }
        }
    }

    void setKeyNodesSet(KeyNodesSet ns)
    {
        _keyNodesSet = SVFUtil::move(ns);
    }

    void clearKeyNodesSet()
    {
        _keyNodesSet.clear();
    }

    inline const Z3Expr &getBranchCondition() const
    {
        return _branchCondition;
    }

    inline void setBranchCondition(const Z3Expr &br)
    {
        _branchCondition = br;
    }

    const TypeState &getAbstractState() const
    {
        return _typeState;
    }

    TypeState &getAbstractState()
    {
        return _typeState;
    }

    void setAbsState(const TypeState &absState)
    {
        _typeState = absState;
    }

    const ConsExeState &getExecutionState() const
    {
        return _exeState;
    }

    ConsExeState &getExecutionState()
    {
        return _exeState;
    }

    /// Overloading Operator==
    inline bool operator==(const SymState &rhs) const
    {
        return _typeState == rhs.getAbstractState() && _exeState == rhs.getExecutionState();
    }

    /// Overloading Operator!=
    inline bool operator!=(const SymState &rhs) const
    {
        return !(*this == rhs);
    }

    /// Overloading Operator==
    inline bool operator<(const SymState &rhs) const
    {
        if (_typeState != rhs.getAbstractState())
            return _typeState < rhs.getAbstractState();
        if (_exeState != rhs.getExecutionState())
            return _exeState < rhs.getExecutionState();
        return false;
    }

    inline bool isNullSymState() const
    {
        return getExecutionState().isNullState() && getAbstractState() == TypeState::Unknown;
    }

};

} // end namespace SVF



/// Specialise hash for SymState
template<>
struct std::hash<SVF::SymState>
{
    size_t operator()(const SVF::SymState &symState) const
    {

        SVF::Hash<std::pair<SVF::TypeState, SVF::ConsExeState>> pairH;

        return pairH(make_pair(symState.getAbstractState(), symState.getExecutionState()));
    }
};

#endif //PSTA_SYMSTATE_H
