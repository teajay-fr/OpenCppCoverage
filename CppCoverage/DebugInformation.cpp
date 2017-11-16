// OpenCppCoverage is an open source code coverage for C++.
// Copyright (C) 2014 OpenCppCoverage
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
#include "DebugInformation.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/optional/optional.hpp>
#include <boost/uuid/uuid_generators.hpp>

#include "Tools/DbgHelp.hpp"
#include "cvconst.h"
#include "Tools/Tool.hpp"
#include "Tools/Log.hpp"
#include "Tools/ScopedAction.hpp"

#include "CppCoverageException.hpp"
#include "ICoverageFilterManager.hpp"

#include "FileFilter/ModuleInfo.hpp"
#include <iostream>

namespace CppCoverage
{
	namespace
	{
		//---------------------------------------------------------------------
		struct Context
		{
			Context(
				HANDLE hProcess,
				DWORD64 baseAddress,
				void* processBaseOfImage,
				const boost::uuids::uuid& moduleUniqueId,
				FileDebugInformation& fileDebugInformation,
				ICoverageFilterManager& coverageFilterManager,
				IDebugInformationEventHandler& debugInformationEventHandler)
				: moduleInfo_{hProcess, moduleUniqueId, processBaseOfImage, baseAddress}
				, fileDebugInformation_{ fileDebugInformation }
				, coverageFilterManager_(coverageFilterManager)
				, debugInformationEventHandler_(debugInformationEventHandler)
			{
			}

			FileFilter::ModuleInfo moduleInfo_;
			FileDebugInformation& fileDebugInformation_;
			IDebugInformationEventHandler& debugInformationEventHandler_;
			ICoverageFilterManager& coverageFilterManager_;
			boost::optional<std::wstring> error_;
		};

		//---------------------------------------------------------------------
	/*	BOOL CALLBACK SymEnumSourceFilesProc(PSOURCEFILE pSourceFile, PVOID userContext)			
		{
			auto context = static_cast<Context*>(userContext);

			if (!userContext)
			{
				LOG_ERROR << L"Invalid user context.";
				return FALSE;
			}
			
			context->error_ = Tools::Try([&]()
			{
				if (!pSourceFile)
					THROW("Source File is null");

				auto filename = Tools::LocalToWString(pSourceFile->FileName);

				if (context->coverageFilterManager_.IsSourceFileSelected(filename))
				{
					context->fileDebugInformation_.LoadFile(
						context->moduleInfo_,
						filename,
						context->coverageFilterManager_,
						context->debugInformationEventHandler_);
				}
			});
			return context->error_ ? FALSE : TRUE;
		}
        */
		//-------------------------------------------------------------------------
		BOOL CALLBACK SymRegisterCallbackProc64(
			_In_      HANDLE hProcess,
			_In_      ULONG actionCode,
			_In_opt_  ULONG64 callbackData,
			_In_opt_  ULONG64 userContext
			)
		{
			if (actionCode == CBA_DEBUG_INFO)
			{
				std::string message = reinterpret_cast<char*>(callbackData);
				
				boost::algorithm::replace_last(message, "\n", "");
				LOG_DEBUG << message;
				return TRUE;
			}

			return FALSE;
		}
        struct CImageHlpLine64 : public IMAGEHLP_LINE64
        {
            CImageHlpLine64()
            {
                SizeOfStruct = sizeof(IMAGEHLP_LINE64);
            }
        };

        BOOL CALLBACK SymEnumFunctionsProc(
            _In_     PSYMBOL_INFO pSymInfo,
            _In_     ULONG        SymbolSize,
            _In_opt_ PVOID        userContext
        )
        {
            auto context = static_cast<Context*>(userContext);

            if (!userContext)
            {
                LOG_ERROR << L"Invalid user context.";
                return FALSE;
            }

            context->error_ = Tools::Try([&]()
            {
                if (pSymInfo->Tag == SymTagEnum::SymTagFunction)
                {
                    CImageHlpLine64 lineInfo;
                    DWORD lineDisplacement = 0; // Displacement from the beginning of the line 

                    if (SymGetLineFromAddr64(
                        context->moduleInfo_.hProcess_, // Process handle of the current process 
                        pSymInfo->Address,  // Address 
                        &lineDisplacement, // Displacement will be stored here by the function 
                        &lineInfo          // File name / line information will be stored here 
                    ))
                    {
                        auto filename = Tools::LocalToWString(lineInfo.FileName);

                        if (context->coverageFilterManager_.IsSourceFileSelected(filename))
                        {
                            context->fileDebugInformation_.LoadFunction(
                                context->moduleInfo_,
                                filename,
                                pSymInfo,
                                &lineInfo,
                                context->coverageFilterManager_,
                                context->debugInformationEventHandler_);
                        }
                    }
                }
            });
            return context->error_ ? FALSE : TRUE;
        }
	}
	
	//-------------------------------------------------------------------------
	DebugInformation::DebugInformation(HANDLE hProcess)
		: hProcess_(hProcess)
	{		
		SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES
				| SYMOPT_NO_UNQUALIFIED_LOADS | SYMOPT_UNDNAME | SYMOPT_DEBUG);
						
		if (!SymInitialize(hProcess_, nullptr, FALSE))
			THROW("Error when calling SymInitialize. You cannot call this function twice.");
		
		if (!SymRegisterCallback64(hProcess_, SymRegisterCallbackProc64, 0))
			THROW("Error when calling SymRegisterCallback64");

		std::vector<char> searchPath(PathBufferSize);
		if (!SymGetSearchPath(hProcess_, &searchPath[0], static_cast<int>(searchPath.size())))
			THROW_LAST_ERROR("Error when calling SymGetSearchPath", GetLastError());
		defaultSearchPath_ = &searchPath[0];
	}
	
	//-------------------------------------------------------------------------
	DebugInformation::~DebugInformation()
	{
		if (!SymCleanup(hProcess_))
		{
			LOG_ERROR << "Error in SymCleanup";
		}
	}
	
	//-------------------------------------------------------------------------
    FileFilter::ModuleInfo DebugInformation::LoadModule(
		const std::wstring& filename,
		HANDLE hFile,
		void* baseOfImage,
		ICoverageFilterManager& coverageFilterManager,
		IDebugInformationEventHandler& debugInformationEventHandler)
	{		
		UpdateSearchPath(filename);
		auto baseAddress = SymLoadModuleEx(hProcess_, hFile, nullptr, nullptr, 0, 0, nullptr, 0);

		if (!baseAddress)
			THROW("Cannot load module for: " << filename);

		Tools::ScopedAction scopedAction{ [=] {
			if (!SymUnloadModule64(hProcess_, baseAddress))
				THROW("UnloadModule64 ");
		} };

		auto moduleUniqueId = boost::uuids::random_generator()();
		Context context{ hProcess_, baseAddress, baseOfImage, moduleUniqueId, fileDebugInformation_,
			coverageFilterManager, debugInformationEventHandler };

        if (!SymEnumSymbols(hProcess_, baseAddress, nullptr, SymEnumFunctionsProc, &context))
        {
            LOG_WARNING << L"Failed to enumerate functions for " << filename;
        }
        if (context.error_)
            throw std::runtime_error(Tools::ToLocalString(*context.error_));

	/*	if (!SymEnumSourceFiles(hProcess_, baseAddress, nullptr, SymEnumSourceFilesProc, &context))
			LOG_WARNING << L"Cannot find pdb for " << filename;
		if (context.error_)
			throw std::runtime_error(Tools::ToLocalString(*context.error_));
        
      */
        return context.moduleInfo_;
	}

	//-------------------------------------------------------------------------
	void DebugInformation::UpdateSearchPath(const std::wstring& moduleFilename) const
	{
		auto parentPath = boost::filesystem::path(moduleFilename).parent_path();

		auto searchPath = defaultSearchPath_ + ';' + parentPath.string();
		if (!SymSetSearchPath(hProcess_, searchPath.c_str()))
			THROW_LAST_ERROR("Error when calling SymSetSearchPath", GetLastError());
	}
}
