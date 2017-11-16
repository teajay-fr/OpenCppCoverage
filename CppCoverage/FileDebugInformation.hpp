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

#pragma once

#include "CppCoverageExport.hpp"
#include <Windows.h>
#include "Tools/DbgHelp.hpp"
#include <boost/flyweight.hpp>
#include <set>

namespace FileFilter
{
	class ModuleInfo;
}

namespace CppCoverage
{
	class IDebugInformationEventHandler;
	class ICoverageFilterManager;

	class CPPCOVERAGE_DLL FileDebugInformation
	{
	public:	
		FileDebugInformation() = default;

	/*	void LoadFile(
			const FileFilter::ModuleInfo&,
			const std::wstring& filename,
			ICoverageFilterManager& coverageFilterManager,
			IDebugInformationEventHandler& debugInformationEventHandler) const;*/
        void LoadFunction(const FileFilter::ModuleInfo&,
            const std::wstring &filename,
            PSYMBOL_INFO symbol,
            PIMAGEHLP_LINE64 sourceLineInfo,
            ICoverageFilterManager& coverageFilterManager,
            IDebugInformationEventHandler& debugInformationEventHandler);
	private:
		FileDebugInformation(const FileDebugInformation&) = delete;
		FileDebugInformation& operator=(const FileDebugInformation&) = delete;        
        std::set<boost::flyweight<std::wstring>> knownFiles;
        std::set<boost::flyweight<std::wstring>> knownScopes;
	};
}
