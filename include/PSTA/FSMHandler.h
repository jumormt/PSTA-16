//
// Created by Xiao on 7/21/2022.
//

#ifndef PSA_ABSTRANSITIONHANDLER_H
#define PSA_ABSTRANSITIONHANDLER_H

#include "SVFIR/SVFValue.h"
#include "Util/cJSON.h"

#include "Util/SVFUtil.h"
#include "SVFIR/SVFIR.h"
#include "Graphs/SVFG.h"
#include <SABER/SaberSVFGBuilder.h>
#include "AE/Core/SymState.h"

#define CHECKERAPI_JSON_PATH "/include/PSTA/CheckerAPI.json"

/*!
 * In this file, we have class:
 * Abstract state transition function handler: FSMHandler
 * FSM parser: FSMParser
 */


namespace SVF {
/*!
 * FSM parser
 *
 * Building a FSM from file
 */
class FSMParser {
public:
    enum CHECKER_TYPE {
        CK_DUMMY = 0,        /// dummy type
        CK_RET,        /// return
        CK_ALLOC,        /// memory allocation
        CK_ALLOC_WRAPPER, /// memory allocation wrapper
        CK_FREE,      /// memory deallocation
        CK_FOPEN,        /// File open
        CK_FCLOSE,        /// File close
        CK_USE_WRAPPER,   /// Use wrapper
        CK_FREE_WRAPPER,    ///Free wrapper
        CK_USE              /// use
    };

    typedef CHECKER_TYPE FSMAction;
    typedef Map<TypeState, Map<FSMAction, TypeState>> FSM;
    typedef Set<TypeState> TypeStates;
    typedef Set<FSMAction> FSMActions;

private:
    FSM _fsm;
    TypeStates _typestates;
    FSMActions _fsmActions;
    TypeState _err;
    TypeState uninit;
    FSMAction _srcAction;

    Map<std::string, CHECKER_TYPE> _typeMap = {
            {"CK_DUMMY",         CK_DUMMY},
            {"CK_RET",           CK_RET},
            {"CK_ALLOC",         CK_ALLOC},
            {"CK_ALLOC_WRAPPER", CK_ALLOC_WRAPPER},
            {"CK_FREE",          CK_FREE},
            {"CK_FOPEN",         CK_FOPEN},
            {"CK_FCLOSE",        CK_FCLOSE},
            {"CK_USE_WRAPPER",   CK_USE_WRAPPER},
            {"CK_FREE_WRAPPER",  CK_FREE_WRAPPER},
            {"CK_USE",           CK_USE}
    };

    static cJSON *_root;

    static std::unique_ptr<FSMParser> _fsmParser;

    /// Constructor
    explicit FSMParser(const std::string &fsmFile);

public:


    /// Singleton
    //{%
    static inline const std::unique_ptr<FSMParser> &createFSMParser(const std::string &_fsmFile) {
        if (_fsmParser == nullptr) {
            _fsmParser = std::unique_ptr<FSMParser>(new FSMParser(_fsmFile));
        }
        return _fsmParser;
    }

    static inline const std::unique_ptr<FSMParser> &getFSMParser() {
        assert(_fsmParser && "fsmparser not init?");
        return _fsmParser;
    }
    //%}



    virtual ~FSMParser() {
        if (_root != nullptr) {
            cJSON_Delete(_root);
            _root = nullptr;
        }
    }

    inline void clearFSM() {
        _fsm.clear();
        _typestates.clear();
    }

    void buildFSM(const std::string &_fsmFile);

    inline const FSM &getFSM() const {
        return _fsm;
    }

    inline const TypeStates &getAbsStates() const {
        return _typestates;
    }

    inline const FSMActions &getFSMActions() const {
        return _fsmActions;
    }

    inline const TypeState &getErrAbsState() const {
        return _err;
    }

    inline const TypeState &getUninitAbsState() const {
        return uninit;
    }

    inline FSMAction getSrcAction() const {
        return _srcAction;
    }

    /// Get the function type of a function
    inline FSMAction getTypeFromStr(const std::string &funName) const {
        return get_type(funName);
    }

private:
    cJSON *get_FunJson(const std::string &funName) const;

    FSMAction get_type(const std::string &funName) const;

    FSMAction get_type(const SVFFunction *F);

    std::string get_name(const SVFFunction *F);

}; // end class FSMParser



/*!
 * Abstract state transition function handler
 *
 * Init sources (e.g., malloc) based on FSM and ICFG node's
 * abstract state transition function based on FSM and alias analysis
 */
class FSMHandler {
public:
    typedef Set<const SVFGNode *> SrcSet;
    typedef Set<const CallICFGNode *> CallSiteSet;
    typedef Map<TypeState, TypeState> TransferFunc;
    typedef OrderedMap<const ICFGNode *, TransferFunc> ICFGAbsTransitionFunc;
    typedef OrderedSet<const ICFGNode *> ICFGNodeSet;

private:
    static std::unique_ptr<FSMHandler> absTransitionHandler;
    SaberSVFGBuilder _svfgBuilder;
    SVFG *_svfg{nullptr};

    FSMHandler() = default;

private:
    ICFGAbsTransitionFunc icfgAbsTransitionFunc; ///< For APIs transition func is set on Callsites
    Set<const SVFGNode *> reachGlobalNodes; ///< Abstract state transfer function for each ICFG node
    const SVFFunction* _mainFunc{nullptr};

public:

    /// Singleton
    static inline const std::unique_ptr<FSMHandler> &getAbsTransitionHandler() {
        if (absTransitionHandler == nullptr) {
            absTransitionHandler = std::unique_ptr<FSMHandler>(new FSMHandler());
        }
        return absTransitionHandler;
    }

    void initMainFunc(SVFModule *module);

    inline const SVFFunction* getMainFunc() const {
        return _mainFunc;
    }

    /// Identify allocation wrappers
    void initSrcs(SrcSet &srcs);

    /// Determine whether a SVFGNode n is in a allocation wrapper function
    bool isInAWrapper(const SVFGNode *src, CallSiteSet &csIdSet, SVFG *svfg, PTACallGraph *ptaCallGraph);

    static const TypeState &
    absStateTransition(const TypeState &curAbsState, const FSMParser::FSMAction &action);

    /// Annotate ICFG with abstract state transfer function
    void
    initAbsTransitionFuncs(const SVFGNode *src, Set<const SVFFunction *> &curEvalFuns, bool recordGlobal = true);

    /// Init object to ICFG sinks map, e.g., UAFFunc
    void initSnks(const SVFGNode *src, ICFGNodeSet &snks, Set<FSMParser::CHECKER_TYPE> &checkerTypes, bool noLimit = false);

    ///  For APIs transition func is set on Callsites
    inline const ICFGAbsTransitionFunc &getICFGAbsTransferMap() const {
        return icfgAbsTransitionFunc;
    }

    inline void clearICFGAbsTransferMap() {
        icfgAbsTransitionFunc.clear();
    }

    inline bool reachGlobal(const SVFGNode *svfgNode) const {
        return reachGlobalNodes.find(svfgNode) != reachGlobalNodes.end();
    }

public:

    void buildOutToIns(SVFG *svfg);

    void buildInToOuts(SVFG *svfg);

    Set<const SVFGNode *> &getReachableActualInsOfActualOut(const SVFGNode *actualOut) {
        return _outToIns[actualOut];
    }

    Set<const SVFGNode *> &getReachableActualOutsOfActualIn(const SVFGNode *actualIn) {
        return _inToOuts[actualIn];
    }

public:
    /// Get the function type of a function
    inline FSMParser::FSMAction getTypeFromFunc(const SVFFunction *func) const {
        FSMParser::FSMAction type = FSMParser::getFSMParser()->getTypeFromStr(func->getName());
        if (type == FSMParser::CK_DUMMY) {
            if (SVFUtil::isExtCall(func) && FSMParser::getFSMParser()->getFSMActions().count(FSMParser::CK_USE)) {
                return FSMParser::CK_USE;
            }
        }
        return type;
    }

private:
    void computeOutToIns(const SVFGNode *src, Set<const SVFGNode *> &visitedOuts);

    void computeInToOuts(const SVFGNode *src, Set<const SVFGNode *> &visitedIns);

private:
    Map<u32_t, Set<const SVFVar *>> _formalParamToVars;
    Map<const SVFGNode *, Set<const SVFGNode *>> _outToIns; // maps an actual out/ret SVFGNode to its reachable actual in/param SVFGNodes
    Map<const SVFGNode *, Set<const SVFGNode *>> _inToOuts; // maps an actual out/ret SVFGNode to its reachable actual in/param SVFGNodes

}; // end class FSMHandler
} // end namespace SVF



#endif //PSA_ABSTRANSITIONHANDLER_H
