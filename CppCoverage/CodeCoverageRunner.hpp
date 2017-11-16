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

#pragma once

#include <memory>
#include <unordered_map>

#include "CoverageData.hpp"
#include "IDebugEventsHandler.hpp"
#include "IDebugInformationEventHandler.hpp"
#include "CppCoverageExport.hpp"

namespace CppCoverage
{
	class StartInfo;
	class RunCoverageSettings;
	class DebugInformation;
	class DebugInformationEventHandler;
	class ExecutedAddressManager;
	class BreakPoint;
	class CoverageFilterManager;
	class ExceptionHandler;
	class UnifiedDiffSettings;

	class CPPCOVERAGE_DLL CodeCoverageRunner : private IDebugEventsHandler, private IDebugInformationEventHandler
	{
	public:
		CodeCoverageRunner();
		~CodeCoverageRunner();

		CoverageData RunCoverage(const RunCoverageSettings&);
		size_t GetDebugInformationCount() const;

	private:
		virtual void OnCreateProcess(const CREATE_PROCESS_DEBUG_INFO&) override;
		virtual void OnExitProcess(HANDLE hProcess, HANDLE hThread, const EXIT_PROCESS_DEBUG_INFO&) override;
		virtual void OnLoadDll(HANDLE hProcess, HANDLE hThread, const LOAD_DLL_DEBUG_INFO&) override;
		virtual void OnUnloadDll(HANDLE hProcess, HANDLE hThread, const UNLOAD_DLL_DEBUG_INFO&) override;
		virtual ExceptionType OnException(HANDLE hProcess, HANDLE hThread, const EXCEPTION_DEBUG_INFO&) override;

	private:
        virtual void OnNewFile(const std::wstring &fileName) override;
        virtual void OnNewClass(const std::wstring &className) override;
        virtual void OnNewFunction(const std::wstring &fileName, const std::wstring &className, const std::wstring &functionName) override;
        virtual void OnNewLine(const SourceCodeLocation &location ) override;
        virtual void OnNewConditional(const SourceCodeLocation& location, const Address &branchAddress) override;
	private:
		CodeCoverageRunner(const CodeCoverageRunner&) = delete;
		CodeCoverageRunner& operator=(const CodeCoverageRunner&) = delete;

		void LoadModule(HANDLE hProcess, HANDLE hFile, void* baseOfImage);
		bool OnBreakPoint(const EXCEPTION_DEBUG_INFO&, HANDLE hProcess, HANDLE hThread);

	private:
        
		std::unordered_map<HANDLE, std::unique_ptr<DebugInformation>> debugInformation_;
		std::unique_ptr<BreakPoint> breakpoint_;
		std::unique_ptr<ExecutedAddressManager> executedAddressManager_;
		std::unique_ptr<CoverageFilterManager> coverageFilterManager_;
		std::unique_ptr<ExceptionHandler> exceptionHandler_;
	};
}


