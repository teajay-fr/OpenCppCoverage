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
#include <string>

namespace FileFilter
{
	class ModuleInfo;
	class FileInfo;
	class LineInfo;
}

namespace CppCoverage
{
	class CoverageFilterSettings;

	class CPPCOVERAGE_DLL ICoverageFilterManager
	{
	public:
		virtual ~ICoverageFilterManager() = default;

		virtual bool IsModuleSelected(const std::wstring& filename) const = 0;
		virtual bool IsSourceFileSelected(const std::wstring& filename) = 0;
		virtual bool IsLineSelected(
			const FileFilter::ModuleInfo&,
			const FileFilter::FileInfo&,
            const std::wstring&,
			const FileFilter::LineInfo&) = 0;
	};
}


