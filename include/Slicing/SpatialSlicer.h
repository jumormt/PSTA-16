//
// Created by Xiao on 7/24/2022.
//

#ifndef PSA_SPATIALSLICER_H
#define PSA_SPATIALSLICER_H

#include "PSTA/FSMHandler.h"
#include "Slicing/ICFGWrapper.h"
#include "Slicing/PIExtractor.h"
#include <Util/DPItem.h>

namespace SVF {

/*!
 * Spatial slicing
 *
 * Extract control and data dependence on the program dependence graph
 */
class SpatialSlicer {

public:
    typedef FSMHandler::SrcSet SrcSet;
    typedef PIExtractor::PI SQ;
    typedef PIExtractor::SrcToPI SrcToSQ;
    typedef PIExtractor::NodeIDSet NodeIDSet;
    typedef FSMHandler::ICFGNodeSet ICFGNodeSet;
    typedef FSMHandler::ICFGAbsTransitionFunc ICFGAbsTransitionFunc;
    typedef Map<const SVFGNode *, Set<CxtDPItem>> SrcToCxtDPItemSetMap;
    typedef PIState::DataFact KeyNodes;


private:
    NodeIDSet &_temporalSlice;       ///< map source object (SVFGNode) to its temporal slice
    NodeIDSet &_callsites;       ///< map source object (SVFGNode) to its passing callsites
    NodeIDSet &_spatialSlice;       ///< map source object (SVFGNode) to its spatio slice
    Set<CxtDPItem> &_cGDpItems;
    NodeIDSet &_branch;    ///< map source object (SVFGNode) to the branches needed for analysis
    SQ &_sQ;                  ///< map source object (SVFGNode) to its operation sequences
    NodeIDSet &_globVars;

    SVFGBuilder _svfgBuilder;
    SVFG *_svfg;

    ICFGNodeWrapper *_curICFGNode;
    SVFGNode *_curSVFGNode;

    Set<const SVFFunction *> _curEvalFuns;
    const ICFGNode *_curEvalICFGNode{nullptr};

public:
    SpatialSlicer(NodeIDSet &temporalSlice, NodeIDSet &callsites,
                  NodeIDSet &spatialSlice, Set<CxtDPItem> &cGDpItems,
                  NodeIDSet &branch, SQ &sQ, NodeIDSet &globVars)
            : _temporalSlice(temporalSlice), _callsites(callsites), _spatialSlice(spatialSlice), _cGDpItems(cGDpItems),
              _branch(branch), _sQ(sQ), _globVars(globVars) {}

    virtual ~SpatialSlicer() = default;

    static inline PTACallGraph *getPTACallGraph() {
        return AndersenWaveDiff::createAndersenWaveDiff(PAG::getPAG())->getPTACallGraph();
    }

    static inline const std::unique_ptr<FSMHandler> &getAbsTransitionHandler() {
        return FSMHandler::getAbsTransitionHandler();
    }

    static inline const std::unique_ptr<ICFGWrapper> &getICFGWrapper() {
        return ICFGWrapper::getICFGWrapper();
    }

    static inline const std::unique_ptr<FSMParser> &getFSMParser() {
        return FSMParser::getFSMParser();
    }


    /// Extract dependent vars in global node
    void extractGlobVars(const SVFGNode *node, Set<u32_t> &globVars, Set<u32_t> &visitedVFNodes);

    /// Multi-point slicing
    //{%
    /// Spatial slicing
    void spatialSlicing(const SVFGNode *src, ICFGNodeSet &snks);
    //%}

    void callsitesExtraction(Set<CxtDPItem> &cGDpItems, Set<u32_t> &callSites, Set<CxtDPItem> &visited);

protected:
    void initLayerAndNs(std::vector<CxtDPItem> &workListLayer,
                        ICFGAbsTransitionFunc &icfgTransferFunc, ICFGNodeSet &snks);

    void controlSlicing(const CxtDPItem &curNode, std::vector<CxtDPItem> &tmpLayer,
                        Set<u32_t> &visitedVFNodes, Set<CxtDPItem> &visitedCallSites,
                        Set<u32_t> &visited);

    void extCallSlicing(const CxtDPItem &curNode, std::vector<CxtDPItem> &tmpLayer,
                        Set<u32_t> &visitedVFNodes, Set<u32_t> &visited);

    void gepSlicing(const CxtDPItem &curNode, std::vector<CxtDPItem> &tmpLayer,
                    Set<u32_t> &visitedVFNodes, Set<u32_t> &visited);

    void dataSlicing(const CxtDPItem &curNode, std::vector<CxtDPItem> &tmpLayer,
                     Set<u32_t> &visitedVFNodes, Set<CxtDPItem> &visitedCallGDPItems,
                     Set<u32_t> &visited);

}; // end class SpatialSlicer
} // end namespace SVF

#endif //PSA_SPATIALSLICER_H
