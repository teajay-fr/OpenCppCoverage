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

#include <string>
#include <Windows.h>
#include "CppCoverageExport.hpp"

namespace CppCoverage
{
	class SourceCodeLocation;
    class Address;
    
    class CPPCOVERAGE_DLL IDebugInformationEventHandler
	{
	public:
		IDebugInformationEventHandler() = default;
		virtual ~IDebugInformationEventHandler() = default;
        virtual void OnNewFile(const std::wstring &fileName) = 0;
        virtual void OnNewClass(const std::wstring &className) = 0;
        virtual void OnNewFunction(const std::wstring &fileName, const std::wstring &className, const std::wstring &functionName) = 0;
		virtual void OnNewLine(const SourceCodeLocation &) = 0;
        virtual void OnNewConditional(const SourceCodeLocation&, const Address &trueBranchAddress, const Address &falseBranchAddress) = 0;
	private:
		IDebugInformationEventHandler(const IDebugInformationEventHandler&) = delete;
		IDebugInformationEventHandler& operator=(const IDebugInformationEventHandler&) = delete;
	};
}


