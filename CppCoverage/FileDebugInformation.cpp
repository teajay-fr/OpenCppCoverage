// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2016 OpenCppCoverage
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "stdafx.h"
#include "FileDebugInformation.hpp"

#include <set>
#include <boost/optional/optional.hpp>

#include "Tools/DbgHelp.hpp"
#include "Tools/Tool.hpp"
#include "Tools/Log.hpp"

#include "CppCoverageException.hpp"
#include "IDebugInformationEventHandler.hpp"
#include "SourceCodeLocation.hpp"
#include "ICoverageFilterManager.hpp"
#include "Address.hpp"

#include "FileFilter/ModuleInfo.hpp"
#include "FileFilter/FileInfo.hpp"
#include "FileFilter/SymbolInfo.hpp"
#include "zydis_wrapper.h"
#include <iostream>
#include <regex>

namespace CppCoverage
{
    namespace
    {
        //-----------------------------------------------------------------------
        struct LineContext
        {
            explicit LineContext(HANDLE hProcess)
                : hProcess_{ hProcess }
            {
            }

            HANDLE hProcess_;
            std::unordered_map<ULONG, FileFilter::SymbolInfo> symbolInfoCollection_;
            boost::optional<std::wstring> error_;
        };

        //-----------------------------------------------------------------------
        bool TryAddLineInfo(const SRCCODEINFO& lineInfo, LineContext& context)
        {
            const int NoSource = 0x00feefee;
            if (lineInfo.LineNumber == NoSource)
                return false;

            char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
            PSYMBOL_INFO symbol = reinterpret_cast<PSYMBOL_INFO>(buffer);

            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = MAX_SYM_NAME;
            if (!SymFromAddr(context.hProcess_, lineInfo.Address, 0, symbol))
                THROW("Error when calling SymFromAddr");
            auto currentSymbolItr = context.symbolInfoCollection_.find(symbol->Index);
            if (currentSymbolItr == context.symbolInfoCollection_.end())
            {
                currentSymbolItr = context.symbolInfoCollection_.emplace(
                    std::pair<ULONG, FileFilter::SymbolInfo>(symbol->Index, *symbol)
                ).first;
            }
            currentSymbolItr->second.lineInfoColllection_.emplace_back(
                FileFilter::LineInfo(lineInfo.LineNumber, lineInfo.Address)
            );

            return true;
        }

        //---------------------------------------------------------------------
        BOOL CALLBACK SymEnumLinesProc(PSRCCODEINFO lineInfo, PVOID context)
        {
            auto lineContext = static_cast<LineContext*>(context);

            if (!lineContext)
            {
                LOG_ERROR << "Invalid user context.";
                return FALSE;
            }

            lineContext->error_ = Tools::Try([&]() {
                TryAddLineInfo(*lineInfo, *lineContext);
            });

            return lineContext->error_ ? FALSE : TRUE;
        }

        //-------------------------------------------------------------------------
        void RetreiveLineData(
            const std::wstring& filename,
            DWORD64 baseAddress,
            LineContext& context)
        {
            auto filenameStr = Tools::ToLocalString(filename);

            if (!SymEnumSourceLines(
                context.hProcess_,
                baseAddress,
                nullptr,
                filenameStr.c_str(),
                0,
                ESLFLAG_FULLPATH,
                SymEnumLinesProc,
                &context))
            {
                THROW("Cannot enumerate source lines for" << filename);
            }
            for (auto &symbolInfo : context.symbolInfoCollection_)
            {
                symbolInfo.second.SortLinesByAddress();
            }
        }

        //-------------------------------------------------------------------------
       /* void HandleNewLine(
            const FileFilter::ModuleInfo& moduleInfo,
            const FileFilter::FileInfo& fileInfo,
            const FileFilter::LineInfo& lineInfo,
            ICoverageFilterManager& coverageFilterManager,
            IDebugInformationEventHandler& debugInformationEventHandler)
        {
            auto lineNumber = lineInfo.lineNumber_;

            if (coverageFilterManager.IsLineSelected(moduleInfo, fileInfo, lineInfo))
            {
                auto addressValue = lineInfo.lineAddress_ - moduleInfo.baseAddress_
                    + reinterpret_cast<DWORD64>(moduleInfo.baseOfImage_);

                Address address{ moduleInfo.hProcess_,  reinterpret_cast<void*>(addressValue) };

               // debugInformationEventHandler.OnNewLine(fileInfo.filePath_.wstring(), lineNumber, address);
            }
        }*/
    }


    std::tuple<std::wstring, std::wstring> SplitScopeAndFunction(const std::wstring &functionName)
    {
        static std::wregex scopeAndFunctionExtractor(L"::(\\w+)$", std::regex_constants::ECMAScript);
        std::wsmatch match;
        if (std::regex_search(functionName, match, scopeAndFunctionExtractor))
        {
            if (match.size() == 2) {
                return std::make_tuple(match.prefix().str(), match[1].str());
            }
        }
        return std::make_tuple(L"_Global_", functionName);
    }
    
    FileDebugInformation::FileDebugInformation()
        : checkStackVarAddress_{}
        , checkStackVarsJumpAddress_{}
    {
    }

    void FileDebugInformation::HandleNewSymbol(const FileFilter::ModuleInfo&module,
        const FileFilter::FileInfo& fileInfo,
        const FileFilter::SymbolInfo &symbol,
        ICoverageFilterManager& coverageFilterManager,
        IDebugInformationEventHandler& debugInformationEventHandler)
    { 
        auto functionScope = SplitScopeAndFunction(symbol.name_);
        boost::flyweight<std::wstring> scopeName(std::get<0>(functionScope));
        if (knownScopes.find(scopeName) == knownScopes.end())
        {
            debugInformationEventHandler.OnNewClass(scopeName);
            knownScopes.insert(scopeName);
        }
        boost::flyweight<std::wstring> functionName(std::get<1>(functionScope));

        debugInformationEventHandler.OnNewFunction(fileInfo.filePath_.native(), scopeName, functionName);
        Zydis disassembler;
        // Create a buffer to copy the code into accessible memory for disassembly
        std::vector<unsigned char> disasmData;
        disasmData.resize(symbol.size_);
        ULONG64 procAddress = symbol.address_ + (ULONG64)module.baseOfImage_ - module.baseAddress_;
        auto result = ReadProcessMemory(module.hProcess_, (void*)procAddress, disasmData.data(), disasmData.size(), nullptr);
        auto currentData = disasmData.data();
        auto dataLeft = disasmData.size();
        ULONG64 pdbProcAddress = symbol.address_;
        ULONG64 procEndAddress = procAddress + symbol.size_;
        bool lastSymbolLineReached = false;
        PCHAR lastFileName = nullptr;
        int lastInstructionSize = 0;
        DWORD64 stackCheckTokenAddress = 0;
        auto lineIterator = symbol.lineInfoColllection_.begin();
        SourceCodeLocation currentSourceLocation(
            boost::flyweight<std::wstring>(fileInfo.filePath_.native()),
            scopeName,
            functionName,
            lineIterator->lineNumber_,
            Address(module.hProcess_, reinterpret_cast<void*>(procAddress))
        );
        if (pdbProcAddress != lineIterator->lineAddress_)
        {
            THROW("PDB line address and symbol disassembly discrepency.");
        }
        bool isLineSelected = coverageFilterManager.IsLineSelected(module, fileInfo, symbol.name_, *lineIterator);
        if (isLineSelected)
        {
            debugInformationEventHandler.OnNewLine(currentSourceLocation);
        }        
        while (dataLeft > 0 && disassembler.Disassemble((size_t)procAddress, currentData, dataLeft) && procAddress < procEndAddress)
        {
            // Are we on another line ?
            if (lineIterator != symbol.lineInfoColllection_.end() && pdbProcAddress >= lineIterator->lineAddress_)
            {                
                if (lineIterator == symbol.lineInfoColllection_.end())
                    isLineSelected = false;
                else
                    isLineSelected = coverageFilterManager.IsLineSelected(module, fileInfo, symbol.name_, *lineIterator);
                if (isLineSelected)
                {
                    currentSourceLocation.lineNumber_ = lineIterator->lineNumber_;
                    if (pdbProcAddress != lineIterator->lineAddress_)
                    {
                        THROW("PDB line address and symbol disassembly discrepency.");
                    }
                    currentSourceLocation.address_ = Address(module.hProcess_, reinterpret_cast<void*>(procAddress));
                    debugInformationEventHandler.OnNewLine(currentSourceLocation);
                }
                lineIterator++;
                
            }
            bool addConditionBreakpoint = true;
            if (checkStackVarsJumpAddress_ == 0 && disassembler.IsBranchType(Zydis::BranchType::BTCallSem))
            {
                DWORD64 jmpTargetData = 0;
                auto target = (void*)disassembler.BranchDestination();
                auto result = ReadProcessMemory(module.hProcess_, target, &jmpTargetData, sizeof(jmpTargetData), nullptr);
                Zydis disassembler2;
                disassembler2.Disassemble(disassembler.BranchDestination(), reinterpret_cast<unsigned char*>(&jmpTargetData), sizeof(jmpTargetData));
                if (disassembler2.IsBranchType(Zydis::BranchType::BTUncondJmpSem))
                {
                    if (disassembler2.BranchDestination() == checkStackVarAddress_)
                    {
                        checkStackVarsJumpAddress_ = disassembler.BranchDestination();
                    }
                }
            }
            if (isLineSelected)
            {
                if (disassembler.IsBranchType(Zydis::BranchType::BTCondJmpSem))
                {
                    void *branchAddress = reinterpret_cast<void*>(procAddress);
                    DWORD displacement = 0;
                    IMAGEHLP_LINE64 line = {};
                    line.SizeOfStruct = sizeof(line);
                    SymGetLineFromAddr64(module.hProcess_, pdbProcAddress, &displacement, &line);
                  
                        // Only add a conditional monitoring break point if the target address is inside of the symbol address range
                        if (disassembler.BranchDestination() <= procEndAddress && procAddress != stackCheckTokenAddress)
                        {
                            debugInformationEventHandler.OnNewConditional(currentSourceLocation, Address(module.hProcess_, branchAddress));
                        }
                    
                }
                
            }

            procAddress += disassembler.Size();
            pdbProcAddress += disassembler.Size();
            dataLeft -= disassembler.Size();
            currentData += disassembler.Size();
            bool enableCheckStackVarLimitCheck = false;
            if ((checkStackVarAddress_ != 0 || checkStackVarsJumpAddress_ != 0) && disassembler.IsCall())
            {
                auto branchTargetAddress = disassembler.BranchDestination();
                if (checkStackVarAddress_ != 0 && branchTargetAddress == checkStackVarAddress_)
                {
                    enableCheckStackVarLimitCheck = true;
                }
                else if (checkStackVarsJumpAddress_ != 0 && branchTargetAddress == checkStackVarsJumpAddress_ ) 
                {
                    enableCheckStackVarLimitCheck = true;
                }
            }
            if (enableCheckStackVarLimitCheck) {
                
                    int callIntructionSize = disassembler.Size();
                    // Jump one instruction back to get the stack check token address.
                    int previousInstructionOffset = callIntructionSize + lastInstructionSize;
                    disassembler.Disassemble((size_t)(procAddress - previousInstructionOffset), currentData- previousInstructionOffset, dataLeft + previousInstructionOffset);
                    lastInstructionSize = callIntructionSize;
                    procEndAddress = disassembler.ResolveOpValue(1, [](ZydisRegister) { return 0; });
            }
            else
            {
                lastInstructionSize = disassembler.Size();
            }           
        }
    }

    //-------------------------------------------------------------------------
    void FileDebugInformation::LoadFile(
        const FileFilter::ModuleInfo& moduleInfo,
        const std::wstring& filePath,
        ICoverageFilterManager& coverageFilterManager,
        IDebugInformationEventHandler& debugInformationEventHandler) 
    {
        LineContext context{ moduleInfo.hProcess_ };

        RetreiveLineData(filePath, moduleInfo.baseAddress_, context);
        if (context.error_)
            throw std::runtime_error(Tools::ToLocalString(*context.error_));
        //Early exit if the line collection is empty
        if (context.symbolInfoCollection_.empty())
            return;

        FileFilter::FileInfo fileInfo{ filePath, std::move(context.symbolInfoCollection_) };

        for (const auto& symbolInfo : fileInfo.symbolInfoCollection_)
        {
            HandleNewSymbol(
                moduleInfo,
                fileInfo,
                symbolInfo.second,
                coverageFilterManager,
                debugInformationEventHandler);
            /*  HandleNewLine(
            moduleInfo,
            fileInfo,
            lineInfo,
            coverageFilterManager,
            debugInformationEventHandler);*/
        }
    }
}
