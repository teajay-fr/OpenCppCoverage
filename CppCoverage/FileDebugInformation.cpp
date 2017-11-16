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
#include "FileFilter/LineInfo.hpp"
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
            std::vector<FileFilter::LineInfo> lineInfoCollection_;
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

            context.lineInfoCollection_.emplace_back(
                lineInfo.LineNumber,
                lineInfo.Address,
                symbol->Index,
                Tools::LocalToWString(symbol->Name));

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
        }

        //-------------------------------------------------------------------------
        void HandleNewLine(
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
        }
    }

    //-------------------------------------------------------------------------
    /*void FileDebugInformation::LoadFile(
        const FileFilter::ModuleInfo& moduleInfo,
        const std::wstring& filePath,
        ICoverageFilterManager& coverageFilterManager,
        IDebugInformationEventHandler& debugInformationEventHandler) const
    {
        LineContext context{ moduleInfo.hProcess_ };

        RetreiveLineData(filePath, moduleInfo.baseAddress_, context);
        if (context.error_)
            throw std::runtime_error(Tools::ToLocalString(*context.error_));

        FileFilter::FileInfo fileInfo{ filePath, std::move(context.lineInfoCollection_) };

        for (const auto& lineInfo : fileInfo.lineInfoColllection_)
        {
            HandleNewLine(
                moduleInfo,
                fileInfo,
                lineInfo,
                coverageFilterManager,
                debugInformationEventHandler);
        }
    }*/
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

    void FileDebugInformation::LoadFunction(const FileFilter::ModuleInfo&module,
        const std::wstring &filename,
        PSYMBOL_INFO symbol,
        PIMAGEHLP_LINE64 sourceLineInfo,
        ICoverageFilterManager& coverageFilterManager,
        IDebugInformationEventHandler& debugInformationEventHandler)
    {
        std::vector<char> symbolBuffer(sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR));
        // Do we already know this file ?
        boost::flyweight<std::wstring> file(filename);
        if (knownFiles.find(file) == knownFiles.end())
        {
            debugInformationEventHandler.OnNewFile(filename);
            knownFiles.insert(file);
        }
        auto functionScope = SplitScopeAndFunction(std::wstring(&symbol->Name[0], &symbol->Name[0] + symbol->NameLen));
        boost::flyweight<std::wstring> scopeName(std::get<0>(functionScope));
        if (knownScopes.find(scopeName) == knownScopes.end())
        {
            debugInformationEventHandler.OnNewClass(scopeName);
            knownScopes.insert(scopeName);
        }
        boost::flyweight<std::wstring> functionName(std::get<1>(functionScope));

        debugInformationEventHandler.OnNewFunction(file, scopeName, functionName);
        Zydis disassembler;
        // Create a buffer to copy the code into accessible memory for disassembly
        std::vector<unsigned char> disasmData;
        disasmData.resize(symbol->Size);
        ULONG64 procAddress = symbol->Address + (ULONG64)module.baseOfImage_ - module.baseAddress_;
        auto result = ReadProcessMemory(module.hProcess_, (void*)procAddress, disasmData.data(), disasmData.size(), nullptr);
        auto currentData = disasmData.data();
        auto dataLeft = disasmData.size();
        ULONG64 pdbProcAddress = symbol->Address;
        ULONG64 procEndAddress = procAddress + symbol->Size;
        bool lastSymbolLineReached = false;
        PCHAR lastFileName = nullptr;
        SourceCodeLocation currentSourceLocation(
            file,
            scopeName,
            functionName,
            sourceLineInfo->LineNumber,
            Address(module.hProcess_, reinterpret_cast<void*>(sourceLineInfo->Address))
            );
        DWORD displacement = 0;
        IMAGEHLP_LINE64 addrLine = {};
        while (dataLeft > 0 && disassembler.Disassemble((size_t)procAddress, currentData, dataLeft))
        {
            if(!SymGetLineFromAddr64(module.hProcess_, pdbProcAddress, &displacement, &addrLine))
            {
                break;
            }            
            bool skip = false;
            if (addrLine.LineNumber != currentSourceLocation.lineNumber_ || addrLine.FileName != lastFileName)
            {
                // Skip any entries which don't correspond to valid line numbers
                const int NoSource = 0x00feefee;
                if (addrLine.LineNumber == NoSource)
                {
                    skip = true;
                }
                else if (coverageFilterManager.IsLineSelected(module, fileInfo, lineInfo))
                {

                    lastFileName = addrLine.FileName;
                    if (addrLine.FileName != lastFileName)
                    {
                        currentSourceLocation.fileName_ = Tools::LocalToWString(addrLine.FileName);
                    }

                    currentSourceLocation.lineNumber_ = addrLine.LineNumber;
                    currentSourceLocation.address_ = Address(module.hProcess_, reinterpret_cast<void*>(procAddress));
                    
                    debugInformationEventHandler.OnNewLine(currentSourceLocation);
                }
            }            
            if (!skip) {
                PSYMBOL_INFO destSymbol = reinterpret_cast<PSYMBOL_INFO>(symbolBuffer.data());
                destSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
                destSymbol->MaxNameLen = MAX_SYM_NAME;
                if (disassembler.IsBranchType(Zydis::BranchType::BTCondJmpSem))
                {
                    void *branchAddress = reinterpret_cast<void*>(procAddress);
                    // Do not add a conditional monitoring break point if the target address is outside of the symbol address range
                    if (disassembler.BranchDestination() <= procEndAddress)
                    {
                        debugInformationEventHandler.OnNewConditional(currentSourceLocation, Address(module.hProcess_, branchAddress));
                    }
                }
            }
            procAddress += disassembler.Size();
            pdbProcAddress += disassembler.Size();
            dataLeft -= disassembler.Size();
            currentData += disassembler.Size();
        }
    }
}
