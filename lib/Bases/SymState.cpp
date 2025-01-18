//
// Created by prophe cheng on 2025/1/17.
//

#include "Bases/SymState.h"

using namespace SVF;

Map<std::string, TypeState> TypeStateParser::_typeMap = {
        {"$uninit",   TypeState::Uinit},
        {"$error",    TypeState::Error},
        {"Allocated", TypeState::Allocated},
        {"Freed",     TypeState::Freed},
        {"Opened",    TypeState::Opened}
};

Map<TypeState, std::string> TypeStateParser::_revTypeMap = {
        {TypeState::Uinit, "$uninit"},
        {TypeState::Error, "$error"},
        {TypeState::Allocated, "Allocated"},
        {TypeState::Freed, "Freed"},
        {TypeState::Opened, "Opened"}
};

SymState::SymState(ConsExeState es, TypeState ts) : _exeState(SVFUtil::move(es)), _typeState(SVFUtil::move(ts)),
                                                    _branchCondition(Z3Expr::getContext().bool_val(true))
{

}