//
// Created by Xiao on 4/17/2022.
//

#ifndef PSA_ICFGWRAPPER_H
#define PSA_ICFGWRAPPER_H

#include <llvm/Support/raw_ostream.h>
#include "SVFIR/SVFIR.h"
#include "Slicing/PIState.h"
#include "Bases/SymState.h"

namespace SVF {
class ICFGNodeWrapper;

typedef GenericEdge<ICFGNodeWrapper> GenericICFGWrapperEdgeTy;


class ICFGEdgeWrapper : public GenericICFGWrapperEdgeTy {
public:
    typedef struct equalICFGEdgeWrapper {
        bool
        operator()(const ICFGEdgeWrapper *lhs, const ICFGEdgeWrapper *rhs) const {
            if (lhs->getSrcID() != rhs->getSrcID())
                return lhs->getSrcID() < rhs->getSrcID();
            else if (lhs->getDstID() != rhs->getDstID())
                return lhs->getDstID() < rhs->getDstID();
            else
                return lhs->getICFGEdge() < rhs->getICFGEdge();
        }
    } equalICFGEdgeWrapper;

    typedef OrderedSet<ICFGEdgeWrapper *, equalICFGEdgeWrapper> ICFGEdgeWrapperSetTy;
    typedef ICFGEdgeWrapperSetTy::iterator iterator;
    typedef ICFGEdgeWrapperSetTy::const_iterator const_iterator;
    typedef PIState::PI SQ;
    typedef PIStateManager::PIStates SQStates;
    typedef OrderedMap<TypeState, PIState> AbsToSQState;

private:
    ICFGEdge *_icfgEdge;

public:
    ICFGEdgeWrapper(ICFGNodeWrapper *src, ICFGNodeWrapper *dst, ICFGEdge *edge) :
            GenericICFGWrapperEdgeTy(src, dst, 0), _icfgEdge(edge) {

    }

    ~ICFGEdgeWrapper() {}

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend llvm::raw_ostream &operator<<(llvm::raw_ostream &o, const ICFGEdgeWrapper &edge) {
        o << edge.toString();
        return o;
    }
    //@}

    virtual const std::string toString() const {
        return _icfgEdge->toString();
    }

    inline ICFGEdge *getICFGEdge() const {
        return _icfgEdge;
    }

    inline void setICFGEdge(ICFGEdge *edge) {
        _icfgEdge = edge;
    }

    /// Add the hash function for std::set (we also can overload operator< to implement this)
    //  and duplicated elements in the set are not inserted (binary tree comparison)
    //@{

    virtual inline bool operator==(const ICFGEdgeWrapper *rhs) const {
        return (rhs->getSrcID() == this->getSrcID() && rhs->getDstID() == this->getDstID() &&
                rhs->getICFGEdge() == this->getICFGEdge());
    }
    //@}

    Map<PIState::DataFact, Set<PIState::DataFact>> _tdDataFactTransferFunc;
    Map<PIState::DataFact, Set<PIState::DataFact>> _buDataFactTransferFunc;
    Map<TypeState, AbsToSQState> _piInfoMap;
    Map<TypeState, Set<TypeState>> _snkInfoMap;

};

typedef GenericNode<ICFGNodeWrapper, ICFGEdgeWrapper> GenericICFGNodeWrapperTy;

class ICFGNodeWrapper : public GenericICFGNodeWrapperTy {
public:
    typedef ICFGEdgeWrapper::ICFGEdgeWrapperSetTy ICFGEdgeWrapperSetTy;
    typedef ICFGEdgeWrapper::ICFGEdgeWrapperSetTy::iterator iterator;
    typedef ICFGEdgeWrapper::ICFGEdgeWrapperSetTy::const_iterator const_iterator;
private:
    const ICFGNode *_icfgNode;
    ICFGNodeWrapper *_callICFGNodeWrapper{nullptr};
    ICFGNodeWrapper *_retICFGNodeWrapper{nullptr};
    ICFGEdgeWrapperSetTy InEdges; ///< all incoming edge of this node
    ICFGEdgeWrapperSetTy OutEdges; ///< all outgoing edge of this node
public:
    ICFGNodeWrapper(const ICFGNode *node) : GenericICFGNodeWrapperTy(node->getId(), 0), _icfgNode(node) {}

    virtual ~ICFGNodeWrapper() {
        for (auto *edge: OutEdges)
            delete edge;
    }

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend llvm::raw_ostream &operator<<(llvm::raw_ostream &o, const ICFGNodeWrapper &node) {
        o << node.toString();
        return o;
    }
    //@}


    virtual const std::string toString() const {
        return _icfgNode->toString();
    }

    const ICFGNode *getICFGNode() const {
        return _icfgNode;
    }

    ICFGNodeWrapper *getCallICFGNodeWrapper() const {
        return _callICFGNodeWrapper;
    }

    void setCallICFGNodeWrapper(ICFGNodeWrapper *node) {
        _callICFGNodeWrapper = node;
    }

    ICFGNodeWrapper *getRetICFGNodeWrapper() const {
        return _retICFGNodeWrapper;
    }

    void setRetICFGNodeWrapper(ICFGNodeWrapper *node) {
        _retICFGNodeWrapper = node;
    }


    /// Get incoming/outgoing edge set
    ///@{
    inline const ICFGEdgeWrapperSetTy &getOutEdges() const {
        return OutEdges;
    }

    inline const ICFGEdgeWrapperSetTy &getInEdges() const {
        return InEdges;
    }
    ///@}

    /// Has incoming/outgoing edge set
    //@{
    inline bool hasIncomingEdge() const {
        return (InEdges.empty() == false);
    }

    inline bool hasOutgoingEdge() const {
        return (OutEdges.empty() == false);
    }
    //@}

    ///  iterators
    //@{
    inline iterator OutEdgeBegin() {
        return OutEdges.begin();
    }

    inline iterator OutEdgeEnd() {
        return OutEdges.end();
    }

    inline iterator InEdgeBegin() {
        return InEdges.begin();
    }

    inline iterator InEdgeEnd() {
        return InEdges.end();
    }

    inline const_iterator OutEdgeBegin() const {
        return OutEdges.begin();
    }

    inline const_iterator OutEdgeEnd() const {
        return OutEdges.end();
    }

    inline const_iterator InEdgeBegin() const {
        return InEdges.begin();
    }

    inline const_iterator InEdgeEnd() const {
        return InEdges.end();
    }
    //@}

    /// Iterators used for SCC detection, overwrite it in child class if necessory
    //@{
    virtual inline iterator directOutEdgeBegin() {
        return OutEdges.begin();
    }

    virtual inline iterator directOutEdgeEnd() {
        return OutEdges.end();
    }

    virtual inline iterator directInEdgeBegin() {
        return InEdges.begin();
    }

    virtual inline iterator directInEdgeEnd() {
        return InEdges.end();
    }

    virtual inline const_iterator directOutEdgeBegin() const {
        return OutEdges.begin();
    }

    virtual inline const_iterator directOutEdgeEnd() const {
        return OutEdges.end();
    }

    virtual inline const_iterator directInEdgeBegin() const {
        return InEdges.begin();
    }

    virtual inline const_iterator directInEdgeEnd() const {
        return InEdges.end();
    }
    //@}

    /// Add incoming and outgoing edges
    //@{
    inline bool addIncomingEdge(ICFGEdgeWrapper *inEdge) {
        return InEdges.insert(inEdge).second;
    }

    inline bool addOutgoingEdge(ICFGEdgeWrapper *outEdge) {
        return OutEdges.insert(outEdge).second;
    }
    //@}

    /// Remove incoming and outgoing edges
    ///@{
    inline u32_t removeIncomingEdge(ICFGEdgeWrapper *edge) {
        iterator it = InEdges.find(edge);
        assert(it != InEdges.end() && "can not find in edge in SVFG node");
        return InEdges.erase(edge);
    }

    inline u32_t removeOutgoingEdge(ICFGEdgeWrapper *edge) {
        iterator it = OutEdges.find(edge);
        assert(it != OutEdges.end() && "can not find out edge in SVFG node");
        return OutEdges.erase(edge);
    }
    ///@}

    /// Find incoming and outgoing edges
    //@{
    inline ICFGEdgeWrapper *hasIncomingEdge(ICFGEdgeWrapper *edge) const {
        const_iterator it = InEdges.find(edge);
        if (it != InEdges.end())
            return *it;
        else
            return nullptr;
    }

    inline ICFGEdgeWrapper *hasOutgoingEdge(ICFGEdgeWrapper *edge) const {
        const_iterator it = OutEdges.find(edge);
        if (it != OutEdges.end())
            return *it;
        else
            return nullptr;
    }
    //@}
public:

    bool _bugReport{false};
    bool _fillingColor{true};
    bool _inTSlice{true};
    bool _inSSlice{true};
    bool _validCS{true};
    bool _inFSM{true};
    bool _isSrcOrSnk{false};
    Set<PIState::DataFact> _tdReachableDataFacts;
    Set<PIState::DataFact> _buReachableDataFacts;
};

typedef std::vector<std::pair<NodeID, NodeID>> NodePairVector;
typedef GenericGraph<ICFGNodeWrapper, ICFGEdgeWrapper> GenericICFGWrapperTy;

class ICFGWrapper : public GenericICFGWrapperTy {
public:

    typedef Map<NodeID, ICFGNodeWrapper *> ICFGWrapperNodeIDToNodeMapTy;
    typedef ICFGEdgeWrapper::ICFGEdgeWrapperSetTy ICFGEdgeWrapperSetTy;
    typedef ICFGWrapperNodeIDToNodeMapTy::iterator iterator;
    typedef ICFGWrapperNodeIDToNodeMapTy::const_iterator const_iterator;
    typedef std::vector<const ICFGNodeWrapper *> ICFGNodeWrapperVector;
    typedef std::vector<std::pair<const ICFGNodeWrapper *, const ICFGNodeWrapper *>> ICFGNodeWrapperPairVector;
    typedef Map<const SVFFunction *, const ICFGNodeWrapper *> SVFFuncToICFGNodeWrapperMap;
    typedef SymState::KeyNodes KeyNodes;
private:
    static std::unique_ptr<ICFGWrapper> _icfgWrapper; ///< Singleton pattern here
    SVFFuncToICFGNodeWrapperMap _funcToFunEntry;
    SVFFuncToICFGNodeWrapperMap _funcToFunExit;
    u32_t _edgeWrapperNum;        ///< total num of node
    u32_t _nodeWrapperNum;        ///< total num of edge
    ICFG *_icfg;

    /// Constructor
    ICFGWrapper(ICFG *icfg) : _icfg(icfg), _edgeWrapperNum(0), _nodeWrapperNum(0) {

    }

public:
    /// Singleton design here to make sure we only have one instance during any analysis
    //@{
    static inline const std::unique_ptr<ICFGWrapper> &getICFGWrapper(ICFG *_icfg) {
        if (_icfgWrapper == nullptr) {
            _icfgWrapper = std::make_unique<ICFGWrapper>(ICFGWrapper(_icfg));
        }
        return _icfgWrapper;
    }

    static inline const std::unique_ptr<ICFGWrapper> &getICFGWrapper() {
        assert(_icfgWrapper && "icfg wrapper not init?");
        return _icfgWrapper;
    }

    static void releaseICFGWrapper() {
        ICFGWrapper *w = _icfgWrapper.release();
        delete w;
        _icfgWrapper = nullptr;
    }
    //@}

    /// Destructor
    virtual ~ICFGWrapper() = default;

    /// Get a ICFG node wrapper
    inline ICFGNodeWrapper *getICFGNodeWrapper(NodeID id) const {
        if (!hasICFGNodeWrapper(id))
            return nullptr;
        return getGNode(id);
    }

    /// Whether has the ICFGNodeWrapper
    inline bool hasICFGNodeWrapper(NodeID id) const {
        return hasGNode(id);
    }

    /// Whether we has a ICFG Edge Wrapper
    bool hasICFGEdgeWrapper(ICFGNodeWrapper *src, ICFGNodeWrapper *dst, ICFGEdge *icfgEdge) {
        ICFGEdgeWrapper edge(src, dst, icfgEdge);
        ICFGEdgeWrapper *outEdge = src->hasOutgoingEdge(&edge);
        ICFGEdgeWrapper *inEdge = dst->hasIncomingEdge(&edge);
        if (outEdge && inEdge) {
            assert(outEdge == inEdge && "edges not match");
            return true;
        } else
            return false;
    }

    ICFGEdgeWrapper *hasICFGEdgeWrapper(ICFGNodeWrapper *src, ICFGNodeWrapper *dst) {
        for (const auto &e: src->getOutEdges()) {
            if (e->getDstNode() == dst)
                return e;
        }
        return nullptr;
    }

    /// Get a ICFG edge wrapper according to src, dst and icfgEdge
    ICFGEdgeWrapper *
    getICFGEdgeWrapper(const ICFGNodeWrapper *src, const ICFGNodeWrapper *dst, ICFGEdge *icfgEdge) {
        ICFGEdgeWrapper *edge = nullptr;
        size_t counter = 0;
        for (ICFGEdgeWrapper::ICFGEdgeWrapperSetTy::iterator iter = src->OutEdgeBegin();
             iter != src->OutEdgeEnd(); ++iter) {
            if ((*iter)->getDstID() == dst->getId()) {
                counter++;
                edge = (*iter);
            }
        }
        assert(counter <= 1 && "there's more than one edge between two ICFGNodeWrappers");
        return edge;
    }

    /// View graph from the debugger
    void view() {
        SVF::ViewGraph(this, "ICFG Wrapper");
    }

    /// Dump graph into dot file
    void dump(const std::string &filename) {
        GraphPrinter::WriteGraphToFile(SVFUtil::outs(), filename, this);
    }

    /// Remove a ICFGEdgeWrapper
    inline void removeICFGEdgeWrapper(ICFGEdgeWrapper *edge) {
        if (edge->getDstNode()->hasIncomingEdge(edge)) {
            edge->getDstNode()->removeIncomingEdge(edge);
        }
        if (edge->getSrcNode()->hasOutgoingEdge(edge)) {
            edge->getSrcNode()->removeOutgoingEdge(edge);
        }
        delete edge;
        _edgeWrapperNum--;
    }

    /// Remove a ICFGNodeWrapper
    inline void removeICFGNodeWrapper(ICFGNodeWrapper *node) {
        std::set<ICFGEdgeWrapper *> temp;
        for (ICFGEdgeWrapper *e: node->getInEdges())
            temp.insert(e);
        for (ICFGEdgeWrapper *e: node->getOutEdges())
            temp.insert(e);
        for (ICFGEdgeWrapper *e: temp) {
            removeICFGEdgeWrapper(e);
        }
        removeGNode(node);
        _nodeWrapperNum--;
    }

    /// Remove node from nodeID
    inline bool removeICFGNodeWrapper(NodeID id) {
        if (hasICFGNodeWrapper(id)) {
            removeICFGNodeWrapper(getICFGNodeWrapper(id));
            return true;
        }
        return false;
    }

    /// Add ICFGEdgeWrapper
    inline bool addICFGEdgeWrapper(ICFGEdgeWrapper *edge) {
        bool added1 = edge->getDstNode()->addIncomingEdge(edge);
        bool added2 = edge->getSrcNode()->addOutgoingEdge(edge);
        assert(added1 && added2 && "edge not added??");
        _edgeWrapperNum++;
        return true;
    }

    /// Add a ICFGNodeWrapper
    virtual inline void addICFGNodeWrapper(ICFGNodeWrapper *node) {
        addGNode(node->getId(), node);
        _nodeWrapperNum++;
    }

    const ICFGNodeWrapper *getFunEntry(const SVFFunction *func) const {
        auto it = _funcToFunEntry.find(func);
        assert(it != _funcToFunEntry.end() && "no entry?");
        return it->second;
    }

    const ICFGNodeWrapper *getFunExit(const SVFFunction *func) const {
        auto it = _funcToFunExit.find(func);
        assert(it != _funcToFunExit.end() && "no exit?");
        return it->second;
    }

    /// Add ICFGEdgeWrappers from nodeid pair
    void addICFGNodeWrapperFromICFGNode(const ICFGNode *src);

    /// Set in N_t flag based on temporal slice
    void annotateTemporalSlice(Set<u32_t> &N_t);

    /// Annotate node on FSM
    void annotateFSMNodes(Set<u32_t> &N);

    /// Set in N_s flag based on spatial slice
    void annotateSpatialSlice(Set<u32_t> &N_s);

    /// Set valid callsites
    void annotateCallsites(Set<u32_t> &N_c);

    /// Remove filled color when dumping dot
    void removeFilledColor();

    /// Set in N_t, N_s and callSites flag for reporting bug
    void annotateMulSlice(Set<u32_t> &N_t, Set<u32_t> &N_s, Set<u32_t> &callSites, const KeyNodes &keyNodes);

    inline u32_t getNodeWrapperNum() const {
        return _nodeWrapperNum;
    }

    inline u32_t getEdgeWrapperNum() const {
        return _edgeWrapperNum;
    }

};

class ICFGWrapperBuilder {
public:
    ICFGWrapperBuilder() {}

    ~ICFGWrapperBuilder() {}

    void build(ICFG *icfg);
};
}

namespace SVF {
/* !
 * GenericGraphTraits specializations for generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph ICFGTraversals.
 */
template<>
struct GenericGraphTraits<SVF::ICFGNodeWrapper *>
        : public GenericGraphTraits<SVF::GenericNode<SVF::ICFGNodeWrapper, SVF::ICFGEdgeWrapper> *> {
};

/// Inverse GenericGraphTraits specializations for call graph node, it is used for inverse ICFGTraversal.
template<>
struct GenericGraphTraits<Inverse<SVF::ICFGNodeWrapper *> > : public GenericGraphTraits<
        Inverse<SVF::GenericNode<SVF::ICFGNodeWrapper, SVF::ICFGEdgeWrapper> *> > {
};

template<>
struct GenericGraphTraits<SVF::ICFGWrapper *>
        : public GenericGraphTraits<SVF::GenericGraph<SVF::ICFGNodeWrapper, SVF::ICFGEdgeWrapper> *> {
    typedef SVF::ICFGNodeWrapper *NodeRef;
};

template<>
struct DOTGraphTraits<SVF::ICFGWrapper *> : public DOTGraphTraits<SVF::SVFIR *> {

    typedef SVF::ICFGNodeWrapper NodeType;

    DOTGraphTraits(bool isSimple = false) :
            DOTGraphTraits<SVF::SVFIR *>(isSimple) {
    }

    /// Return name of the graph
    static std::string getGraphName(SVF::ICFGWrapper *) {
        return "ICFGWrapper";
    }

    static bool isNodeHidden(NodeType *node, SVF::ICFGWrapper *graph) {
        if (node->_bugReport) {
            if (!node->_inSSlice && !node->_inTSlice)
                return true;
        }
        return false;
    }

    std::string getNodeLabel(NodeType *node, SVF::ICFGWrapper *graph) {
        return getSimpleNodeLabel(node, graph);
    }

    /// Return the label of an ICFG node
    static std::string getSimpleNodeLabel(NodeType *node, SVF::ICFGWrapper *) {
        std::string str;
        std::stringstream rawstr(str);
        rawstr << "NodeID: " << node->getId() << "\n";
        if (const SVF::IntraICFGNode *bNode = SVF::SVFUtil::dyn_cast<SVF::IntraICFGNode>(node->getICFGNode())) {
            rawstr << "IntraICFGNode ID: " << bNode->getId() << " \t";
            SVF::SVFIR::SVFStmtList &edges = SVF::SVFIR::getPAG()->getSVFStmtList(bNode);
            if (edges.empty()) {
                rawstr << bNode->getInst()->toString() << " \t";
            } else {
                for (SVF::SVFIR::SVFStmtList::iterator it = edges.begin(), eit = edges.end(); it != eit; ++it) {
                    const SVF::PAGEdge *edge = *it;
                    rawstr << edge->toString();
                }
            }
            rawstr << " {fun: " << bNode->getFun()->getName() << "}";
        } else if (const SVF::FunEntryICFGNode *entry = SVF::SVFUtil::dyn_cast<SVF::FunEntryICFGNode>(
                node->getICFGNode())) {
            rawstr << entry->toString();
        } else if (const SVF::FunExitICFGNode *exit = SVF::SVFUtil::dyn_cast<SVF::FunExitICFGNode>(
                node->getICFGNode())) {
            rawstr << exit->toString();
        } else if (const SVF::CallICFGNode *call = SVF::SVFUtil::dyn_cast<SVF::CallICFGNode>(node->getICFGNode())) {
            rawstr << call->toString();
        } else if (const SVF::RetICFGNode *ret = SVF::SVFUtil::dyn_cast<SVF::RetICFGNode>(node->getICFGNode())) {
            rawstr << ret->toString();
        } else if (const SVF::GlobalICFGNode *glob = SVF::SVFUtil::dyn_cast<SVF::GlobalICFGNode>(
                node->getICFGNode())) {
            SVF::SVFIR::SVFStmtList &edges = SVF::SVFIR::getPAG()->getSVFStmtList(glob);
            for (SVF::SVFIR::SVFStmtList::iterator it = edges.begin(), eit = edges.end(); it != eit; ++it) {
                const SVF::PAGEdge *edge = *it;
                rawstr << edge->toString();
            }
        } else
            assert(false && "what else kinds of nodes do we have??");

        return rawstr.str();
    }

    static std::string getNodeAttributes(NodeType *node, SVF::ICFGWrapper *) {
        std::string str;
        std::stringstream rawstr(str);

        if (SVF::SVFUtil::isa<SVF::IntraICFGNode>(node->getICFGNode())) {
            rawstr << "color=black";
        } else if (SVF::SVFUtil::isa<SVF::FunEntryICFGNode>(node->getICFGNode())) {
            rawstr << "color=yellow";
        } else if (SVF::SVFUtil::isa<SVF::FunExitICFGNode>(node->getICFGNode())) {
            rawstr << "color=green";
        } else if (SVF::SVFUtil::isa<SVF::CallICFGNode>(node->getICFGNode())) {
            rawstr << "color=red";
        } else if (SVF::SVFUtil::isa<SVF::RetICFGNode>(node->getICFGNode())) {
            rawstr << "color=blue";
        } else if (SVF::SVFUtil::isa<SVF::GlobalICFGNode>(node->getICFGNode())) {
            rawstr << "color=purple";
        } else
            assert(false && "no such kind of node!!");

        if (node->_bugReport) {
            if (node->_isSrcOrSnk) {
                rawstr << ", style=filled, fillcolor=red";
            } else if (node->_inFSM)
                rawstr << ", style=filled, fillcolor=pink";
            else if (node->_inTSlice && node->_inSSlice)
                rawstr << ", style=filled, fillcolor=green";
            else if (node->_inTSlice)
                rawstr << ", style=filled, fillcolor=yellow";
            else if (node->_inSSlice)
                rawstr << ", style=filled, fillcolor=gray";


            return rawstr.str();
        }

        if (!node->_fillingColor) {
            if (node->_isSrcOrSnk) {
                rawstr << ", style=filled, fillcolor=red";
            } else if (node->_inFSM)
                rawstr << ", style=filled, fillcolor=pink";
            return rawstr.str();
        }
        if (node->_isSrcOrSnk) {
            rawstr << ", style=filled, fillcolor=red";
        } else if (node->_inFSM)
            rawstr << ", style=filled, fillcolor=pink";
        else if (node->_inTSlice && node->_inSSlice)
            rawstr << ", style=filled, fillcolor=green";
        else if (node->_inTSlice)
            rawstr << ", style=filled, fillcolor=yellow";
        else if (node->_inSSlice)
            rawstr << ", style=filled, fillcolor=gray";

        rawstr << "";

        return rawstr.str();
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(NodeType *, EdgeIter EI, SVF::ICFGWrapper *) {
        SVF::ICFGEdgeWrapper *edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
        if (!edge->getICFGEdge())
            return "style=solid";
        if (SVF::SVFUtil::isa<SVF::CallCFGEdge>(edge->getICFGEdge()))
            return "style=solid,color=red";
        else if (SVF::SVFUtil::isa<SVF::RetCFGEdge>(edge->getICFGEdge()))
            return "style=solid,color=blue";
        else
            return "style=solid";
        return "";
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(NodeType *, EdgeIter EI) {
        SVF::ICFGEdgeWrapper *edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");

        std::string str;
        std::stringstream rawstr(str);
        if (!edge->getICFGEdge())
            return rawstr.str();
        if (SVF::CallCFGEdge *dirCall = SVF::SVFUtil::dyn_cast<SVF::CallCFGEdge>(edge->getICFGEdge()))
            rawstr << dirCall->getCallSite();
        else if (SVF::RetCFGEdge *dirRet = SVF::SVFUtil::dyn_cast<SVF::RetCFGEdge>(edge->getICFGEdge()))
            rawstr << dirRet->getCallSite();

        return rawstr.str();
    }
};

} // End namespace llvm

#endif //PSA_ICFGWRAPPER_H
