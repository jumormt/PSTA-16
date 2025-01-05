/*
 * ControlDG.h
 *
 * Control Dependence Graph, Node contains ICFG Node, Edge is labeled with branch condition
 *
 * Created by xiao on 3/21/22.
 *
 */
#ifndef CDBUILDER_CONTROLDG_H
#define CDBUILDER_CONTROLDG_H

#include <llvm/Support/raw_ostream.h>
#include "SVFIR/SVFIR.h"

namespace SVF {

class ControlDGNode;

typedef GenericEdge<ControlDGNode> GenericControlDGEdgeTy;

class ControlDGEdge : public GenericControlDGEdgeTy {
public:
    typedef std::pair<const SVFValue *, s32_t> BranchCondition;

    /// Constructor
    ControlDGEdge(ControlDGNode *s, ControlDGNode *d) : GenericControlDGEdgeTy(s, d, 0) {
    }

    /// Destructor
    ~ControlDGEdge() {
    }

    typedef GenericNode<ControlDGNode, ControlDGEdge>::GEdgeSetTy ControlDGEdgeSetTy;
    typedef ControlDGEdgeSetTy SVFGEdgeSetTy;

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend llvm::raw_ostream &operator<<(llvm::raw_ostream &o, const ControlDGEdge &edge) {
        o << edge.toString();
        return o;
    }
    //@}

    virtual const std::string toString() const {
        std::string str;
        std::stringstream rawstr(str);
        rawstr << "ControlDGEdge " << " [";
        rawstr << getDstID() << "<--" << getSrcID() << "\t";
        return rawstr.str();
    }

    /// get/set branch condition
    //{@
    const Set<BranchCondition> &getBranchConditions() const {
        return brConditions;
    }

    void insertBranchCondition(const SVFValue *pNode, s32_t branchID) {
        brConditions.insert(std::make_pair(pNode, branchID));
    }
    //@}


private:
    Set<BranchCondition> brConditions;
};

typedef GenericNode<ControlDGNode, ControlDGEdge> GenericControlDGNodeTy;

class ControlDGNode : public GenericControlDGNodeTy {

public:

    typedef ControlDGEdge::ControlDGEdgeSetTy::iterator iterator;
    typedef ControlDGEdge::ControlDGEdgeSetTy::const_iterator const_iterator;

public:
    /// Constructor
    ControlDGNode(const ICFGNode *icfgNode) : GenericControlDGNodeTy(icfgNode->getId(), 0), _icfgNode(icfgNode) {

    }

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend llvm::raw_ostream &operator<<(llvm::raw_ostream &o, const ControlDGNode &node) {
        o << node.toString();
        return o;
    }
    //@}


    virtual const std::string toString() const {
        std::string str;
        std::stringstream rawstr(str);
        rawstr << getId();
        return rawstr.str();
    }

    const ICFGNode *getICFGNode() const {
        return _icfgNode;
    }


private:
    const ICFGNode *_icfgNode;
};

typedef std::vector<std::pair<NodeID, NodeID>> NodePairVector;
typedef GenericGraph<ControlDGNode, ControlDGEdge> GenericControlDGTy;

class ControlDG : public GenericControlDGTy {

public:

    typedef Map<NodeID, ControlDGNode *> ControlDGNodeIDToNodeMapTy;
    typedef ControlDGEdge::ControlDGEdgeSetTy ControlDGEdgeSetTy;
    typedef ControlDGNodeIDToNodeMapTy::iterator iterator;
    typedef ControlDGNodeIDToNodeMapTy::const_iterator const_iterator;
    typedef std::vector<const ICFGNode *> ICFGNodeVector;
    typedef std::vector<std::pair<const ICFGNode *, const ICFGNode *>> ICFGNodePairVector;

private:
    static ControlDG *controlDg; ///< Singleton pattern here
    /// Constructor
    ControlDG() {

    }


public:
    /// Singleton design here to make sure we only have one instance during any analysis
    //@{
    static inline ControlDG *getControlDG() {
        if (controlDg == nullptr) {
            controlDg = new ControlDG();
        }
        return controlDg;
    }

    static void releaseControlDG() {
        if (controlDg)
            delete controlDg;
        controlDg = nullptr;
    }
    //@}

    /// Destructor
    virtual ~ControlDG() {}

    /// Get a ControlDG node
    inline ControlDGNode *getControlDGNode(NodeID id) const {
        if (!hasControlDGNode(id))
            return nullptr;
        return getGNode(id);
    }

    /// Whether has the ControlDGNode
    inline bool hasControlDGNode(NodeID id) const {
        return hasGNode(id);
    }

    /// Whether we has a ControlDG edge
    bool hasControlDGEdge(ControlDGNode *src, ControlDGNode *dst) {
        ControlDGEdge edge(src, dst);
        ControlDGEdge *outEdge = src->hasOutgoingEdge(&edge);
        ControlDGEdge *inEdge = dst->hasIncomingEdge(&edge);
        if (outEdge && inEdge) {
            assert(outEdge == inEdge && "edges not match");
            return true;
        } else
            return false;
    }

    /// Get a control dependence edge according to src and dst
    ControlDGEdge *getControlDGEdge(const ControlDGNode *src, const ControlDGNode *dst) {
        ControlDGEdge *edge = nullptr;
        size_t counter = 0;
        for (ControlDGEdge::ControlDGEdgeSetTy::iterator iter = src->OutEdgeBegin();
             iter != src->OutEdgeEnd(); ++iter) {
            if ((*iter)->getDstID() == dst->getId()) {
                counter++;
                edge = (*iter);
            }
        }
        assert(counter <= 1 && "there's more than one edge between two ControlDG nodes");
        return edge;
    }

    /// View graph from the debugger
    void view() {
        SVF::ViewGraph(this, "Control Dependence Graph");
    }

    /// Dump graph into dot file
    void dump(const std::string &filename) {
        GraphPrinter::WriteGraphToFile(SVFUtil::outs(), filename, this);
    }

public:
    /// Remove a control dependence edge
    inline void removeControlDGEdge(ControlDGEdge *edge) {
        edge->getDstNode()->removeIncomingEdge(edge);
        edge->getSrcNode()->removeOutgoingEdge(edge);
        delete edge;
    }

    /// Remove a ControlDGNode
    inline void removeControlDGNode(ControlDGNode *node) {
        std::set<ControlDGEdge *> temp;
        for (ControlDGEdge *e: node->getInEdges())
            temp.insert(e);
        for (ControlDGEdge *e: node->getOutEdges())
            temp.insert(e);
        for (ControlDGEdge *e: temp) {
            removeControlDGEdge(e);
        }
        removeGNode(node);
    }

    /// Remove node from nodeID
    inline bool removeControlDGNode(NodeID id) {
        if (hasControlDGNode(id)) {
            removeControlDGNode(getControlDGNode(id));
            return true;
        }
        return false;
    }

    /// Add ControlDG edge
    inline bool addControlDGEdge(ControlDGEdge *edge) {
        bool added1 = edge->getDstNode()->addIncomingEdge(edge);
        bool added2 = edge->getSrcNode()->addOutgoingEdge(edge);
        assert(added1 && added2 && "edge not added??");
        return true;
    }

    /// Add a ControlDG node
    virtual inline void addControlDGNode(ControlDGNode *node) {
        addGNode(node->getId(), node);
    }

    /// Add ControlDG nodes from nodeid vector
    inline void addControlDGNodesFromVector(ICFGNodeVector nodes) {
        for (const ICFGNode *icfgNode: nodes) {
            if (!IDToNodeMap.count(icfgNode->getId())) {
                addGNode(icfgNode->getId(), new ControlDGNode(icfgNode));
            }
        }
    }

    /// Add ControlDG edges from nodeid pair
    void addControlDGEdgeFromSrcDst(const ICFGNode *src, const ICFGNode *dst, const SVFValue *pNode, s32_t branchID);

};
} // end namespace SVF

namespace SVF {
/* !
 * GenericGraphTraits specializations for generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph ICFGTraversals.
 */
template<>
struct GenericGraphTraits<SVF::ControlDGNode *>
        : public GenericGraphTraits<SVF::GenericNode<SVF::ControlDGNode, SVF::ControlDGEdge> *> {
};

/// Inverse GenericGraphTraits specializations for call graph node, it is used for inverse ICFGTraversal.
template<>
struct GenericGraphTraits<Inverse<SVF::ControlDGNode *> > : public GenericGraphTraits<
        Inverse<SVF::GenericNode<SVF::ControlDGNode, SVF::ControlDGEdge> *> > {
};

template<>
struct GenericGraphTraits<SVF::ControlDG *>
        : public GenericGraphTraits<SVF::GenericGraph<SVF::ControlDGNode, SVF::ControlDGEdge> *> {
    typedef SVF::ControlDGNode *NodeRef;
};

template<>
struct DOTGraphTraits<SVF::ControlDG *> : public DOTGraphTraits<SVF::PAG *> {

    typedef SVF::ControlDGNode NodeType;

    DOTGraphTraits(bool isSimple = false) :
            DOTGraphTraits<SVF::PAG *>(isSimple) {
    }

    /// Return name of the graph
    static std::string getGraphName(SVF::ControlDG *) {
        return "Control Dependence Graph";
    }

    std::string getNodeLabel(NodeType *node, SVF::ControlDG *graph) {
        return getSimpleNodeLabel(node, graph);
    }

    /// Return the label of an ICFG node
    static std::string getSimpleNodeLabel(NodeType *node, SVF::ControlDG *) {
        std::string str;
        std::stringstream rawstr(str);
        rawstr << "NodeID: " << node->getId() << "\n";
        const SVF::ICFGNode *icfgNode = node->getICFGNode();
        if (const SVF::IntraICFGNode *bNode = SVF::SVFUtil::dyn_cast<SVF::IntraICFGNode>(icfgNode)) {
            rawstr << "IntraBlockNode ID: " << bNode->getId() << " \t";
            SVF::PAG::SVFStmtList &edges = SVF::PAG::getPAG()->getPTASVFStmtList(bNode);
            if (edges.empty()) {
                rawstr << bNode->getInst()->toString() << " \t";
            } else {
                for (SVF::PAG::SVFStmtList::iterator it = edges.begin(), eit = edges.end(); it != eit; ++it) {
                    const SVF::PAGEdge *edge = *it;
                    rawstr << edge->toString();
                }
            }
            rawstr << " {fun: " << bNode->getFun()->getName() << "}";
        } else if (const SVF::FunEntryICFGNode *entry = SVF::SVFUtil::dyn_cast<SVF::FunEntryICFGNode>(icfgNode)) {
            rawstr << entry->toString();
        } else if (const SVF::FunExitICFGNode *exit = SVF::SVFUtil::dyn_cast<SVF::FunExitICFGNode>(icfgNode)) {
            rawstr << exit->toString();
        } else if (const SVF::CallICFGNode *call = SVF::SVFUtil::dyn_cast<SVF::CallICFGNode>(icfgNode)) {
            rawstr << call->toString();
        } else if (const SVF::RetICFGNode *ret = SVF::SVFUtil::dyn_cast<SVF::RetICFGNode>(icfgNode)) {
            rawstr << ret->toString();
        } else if (const SVF::GlobalICFGNode *glob = SVF::SVFUtil::dyn_cast<SVF::GlobalICFGNode>(icfgNode)) {
            SVF::PAG::SVFStmtList &edges = SVF::PAG::getPAG()->getPTASVFStmtList(glob);
            for (SVF::PAG::SVFStmtList::iterator it = edges.begin(), eit = edges.end(); it != eit; ++it) {
                const SVF::PAGEdge *edge = *it;
                rawstr << edge->toString();
            }
        } else
            assert(false && "what else kinds of nodes do we have??");

        return rawstr.str();
    }

    static std::string getNodeAttributes(NodeType *node, SVF::ControlDG *) {
        std::string str;
        std::stringstream rawstr(str);
        const SVF::ICFGNode *icfgNode = node->getICFGNode();

        if (SVF::SVFUtil::isa<SVF::IntraICFGNode>(icfgNode)) {
            rawstr << "color=black";
        } else if (SVF::SVFUtil::isa<SVF::FunEntryICFGNode>(icfgNode)) {
            rawstr << "color=yellow";
        } else if (SVF::SVFUtil::isa<SVF::FunExitICFGNode>(icfgNode)) {
            rawstr << "color=green";
        } else if (SVF::SVFUtil::isa<SVF::CallICFGNode>(icfgNode)) {
            rawstr << "color=red";
        } else if (SVF::SVFUtil::isa<SVF::RetICFGNode>(icfgNode)) {
            rawstr << "color=blue";
        } else if (SVF::SVFUtil::isa<SVF::GlobalICFGNode>(icfgNode)) {
            rawstr << "color=purple";
        } else
            assert(false && "no such kind of node!!");

        rawstr << "";

        return rawstr.str();
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(NodeType *, EdgeIter EI, SVF::ControlDG *) {
        SVF::ControlDGEdge *edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
        return "style=solid";
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(NodeType *, EdgeIter EI) {
        SVF::ControlDGEdge *edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");

        std::string str;
        std::stringstream rawstr(str);
        for (const auto &cond: edge->getBranchConditions()) {
            rawstr << std::to_string(cond.second) << "|";
        }
        std::string lb = rawstr.str();
        lb.pop_back();

        return lb;
    }
};

} // End namespace llvm
#endif //CDBUILDER_CONTROLDG_H
