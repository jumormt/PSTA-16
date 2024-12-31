//
// Created by Xiao on 2022/4/4.
//

#ifndef PSA_SQSTATE_H
#define PSA_SQSTATE_H

#include <WPA/Andersen.h>

#include <utility>
#include <llvm/ADT/SmallVector.h>
#include "SVFIR/SVFIR.h"
#include "Graphs/SVFG.h"
#include "PSTA/FSMHandler.h"
#include "PSTA/PSAOptions.h"

namespace SVF {

/*!
 * Bounded PI
 *
 * @tparam T
 */
template<typename T>
class SeQ {

private:
    OrderedSet<T> _orderedSet;

public:
    /// Constructor
    SeQ() = default;

    /// Destructor
    ~SeQ() = default;

    /// Copy Constructor
    SeQ(const SeQ &rhs) : _orderedSet(rhs._orderedSet) {}

    /// Operator=
    SeQ &operator=(const SeQ &rhs) {
        if (*this != rhs) {
            _orderedSet = rhs._orderedSet;
        }
        return *this;
    }

    /// Move Constructor
    SeQ(SeQ &&rhs) noexcept: _orderedSet(SVFUtil::move(rhs._orderedSet)) {}

    /// Move Operator
    SeQ &operator=(SeQ &&rhs) noexcept {
        if (this != &rhs) {
            _orderedSet = SVFUtil::move(rhs._orderedSet);
        }
        return *this;
    }

    inline bool operator==(const SeQ &rhs) const {
        if(size() != rhs.size()) return false;
        return _orderedSet == rhs._orderedSet;
    }

    inline bool operator!=(const SeQ &rhs) const {
        return !(*this == rhs);
    }

    inline bool empty() const {
        return _orderedSet.empty();
    }

    inline void clear() {
        _orderedSet.clear();
    }

    inline typename OrderedSet<T>::iterator begin() const {
        return _orderedSet.begin();
    }

    inline typename OrderedSet<T>::iterator end() const {
        return _orderedSet.end();
    }

    inline typename OrderedSet<T>::const_iterator cbegin() const {
        return _orderedSet.cbegin();
    }

    inline typename OrderedSet<T>::const_iterator cend() const {
        return _orderedSet.cend();
    }

    inline std::size_t size() const {
        return _orderedSet.size();
    }

    inline std::pair<typename OrderedSet<T>::iterator, bool> insert(const T &data) {
        if (size() < PSAOptions::MaxSQSize())
            return _orderedSet.insert(data);
        else
            return std::make_pair(_orderedSet.end(), false);
    }

    inline std::pair<typename OrderedSet<T>::iterator, bool> insert(T &&data) {
        if (size() < PSAOptions::MaxSQSize())
            return _orderedSet.insert(data);
        else
            return std::make_pair(_orderedSet.end(), false);
    }

    inline void
    insert(typename OrderedSet<T>::iterator st, typename OrderedSet<T>::iterator ed) {
        for (auto it = st; it != ed; ++it) {
            insert(*it);
        }
    }
};

/*!
 * Operation Sequence State
 */
class PIState {
    friend class PIStateManager;
public:

    typedef llvm::SmallVector<u32_t, 4> DataFact;
    typedef SeQ<DataFact> PI;

    PIState() : _absState(TypeState::Unknown) {

    }


    PIState(TypeState as, PI seqs) : _absState(as), _PI(std::move(seqs)) {

    }

    PIState(const PIState &state) : _absState(state._absState), _PI(state._PI) {

    }

    PIState& operator=(const PIState& rhs) {
        if (*this != rhs) {
            _absState = rhs._absState;
            _PI = rhs._PI;
        }
        return *this;
    }

    inline bool operator==(const PIState& rhs) const {
        return _absState == rhs._absState && _PI == rhs._PI;
    }

    inline bool operator!=(const PIState& rhs) const {
        return !(*this == rhs);
    }

    ~PIState() = default;

    PIState(PIState &&rhs) noexcept: _absState(rhs._absState),
                                     _PI(SVFUtil::move(rhs._PI)) {}

    PIState &operator=(PIState &&rhs) noexcept {
        if (this != &rhs) {
            _absState = rhs._absState;
            _PI = SVFUtil::move(rhs._PI);
        }
        return *this;
    }

    const TypeState &getAbstractState() const {
        return _absState;
    }

    const PI &getPI() const {
        return _PI;
    }

    bool isNullPI() const {
        return _absState == TypeState::Unknown;
    }

private:
    TypeState _absState;
    PI _PI;
};

/*!
 * Operation Sequence State Manager
 */
class PIStateManager {

public:
    typedef PIState::DataFact DataFact;
    typedef PIState::PI PI;
    typedef Map<size_t, const PIState *> hashToPIStateMap;
    typedef std::vector<PIState> PIStates;
    typedef OrderedSet<const PIState *> OrderedPIStates;
    typedef FSMHandler::ICFGNodeSet ICFGNodeSet;

private:
    hashToPIStateMap _hashToPIStateMap;
    const ICFGNode *_curEvalICFGNode{nullptr};
    Set<FSMParser::CHECKER_TYPE> _snkTypes;
    ICFGNodeSet _curSnks;

public:

    explicit PIStateManager() = default;

    ~PIStateManager();

    static PTACallGraph *getPTACallGraph() {
        return AndersenWaveDiff::createAndersenWaveDiff(PAG::getPAG())->getPTACallGraph();
    }

    const PIState *getOrAddPIState(PI sqs, TypeState absState);

    static inline const FSMHandler::ICFGAbsTransitionFunc &getICFGAbsTransferMap() {
        return FSMHandler::getAbsTransitionHandler()->getICFGAbsTransferMap();
    }

    /// AbsTransitionHandler singleton
    static inline const std::unique_ptr<FSMHandler> &getFSMHandler() {
        return FSMHandler::getAbsTransitionHandler();
    }

    void nonBranchFlowFun(const ICFGNode *icfgNode, PIState *sqState, const SVFGNode *svfgNode);

    inline void setCurEvalICFGNode(const ICFGNode *icfgNode) {
        _curEvalICFGNode = icfgNode;
    }

    inline void setCheckerTypes(const Set<FSMParser::CHECKER_TYPE> &_checkerTypes) {
        _snkTypes = _checkerTypes;
    }

    inline void setCurSnks(const ICFGNodeSet &curSnks) {
        _curSnks = curSnks;
    }

    void releasePIStates();

    static size_t computeHashOfPIState(const PI &sqs, const TypeState &absState);

    static FSMParser::FSMAction getActionOfICFGNode(const ICFGNode *icfgNode);

    static inline void extractAbsStatesOfPIStates(const PIStates &ss, Set<TypeState> &absStates) {
        for (const auto &s: ss) {
            absStates.insert(s.getAbstractState());
        }
    }
};


}

template<>
struct std::hash<SVF::PIState> {
    std::size_t operator()(const SVF::PIState &sqState) const {
        size_t h = sqState.getPI().size();
        SVF::Hash<SVF::PIState::DataFact> hf;
        for (const auto &t: sqState.getPI()) {
            h ^= hf(t) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        SVF::Hash<std::pair<size_t, SVF::TypeState>> pairH;

        return pairH(std::make_pair(h, sqState.getAbstractState()));
    }
};

template<typename T, unsigned N>
struct std::hash<llvm::SmallVector<T, N>> {
    size_t operator()(const llvm::SmallVector<T, N> &sv) const {
        if (sv.empty()) return 0;
        if (sv.size() == 1) return sv[0];

        // Iterate and accumulate the hash.
        size_t hash = 0;
        SVF::Hash<std::pair<T, size_t>> hts;
        std::hash<T> ht;
        for (const T &t: sv) {
            hash = hts(std::make_pair(ht(t), hash));
        }

        return hash;
    }
};

#endif //PSA_SQSTATE_H
