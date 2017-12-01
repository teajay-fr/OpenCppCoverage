#include "stdafx.h"
#include "SymbolInfo.hpp"
#include "Tools/DbgHelp.hpp"
#include "Tools/Tool.hpp"

namespace FileFilter
{
   SymbolInfo::SymbolInfo()
       : index_{}
       , name_{}
       , address_{}
       , size_{}
    {
    }

    SymbolInfo::SymbolInfo(const _SYMBOL_INFO& symbol)
        : index_(symbol.Index)
        , name_(Tools::LocalToWString(symbol.Name))
        , address_(symbol.Address)
        , size_(symbol.Size)
    {
    }

    void SymbolInfo::SortLinesByAddress()
    {
        std::sort(lineInfoColllection_.begin(), lineInfoColllection_.end(),
            [](const LineInfo&lhs, const LineInfo &rhs) {
            return lhs.lineAddress_ < rhs.lineAddress_;
        });
    }

}