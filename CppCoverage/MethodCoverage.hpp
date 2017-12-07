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
#include <map>
#include <memory>
#include <boost/filesystem.hpp>
#include "CppCoverageExport.hpp"

namespace CppCoverage
{
    class LineCoverage;
	class CPPCOVERAGE_DLL MethodCoverage
	{
	public:
		MethodCoverage(const boost::filesystem::path& filePath, const std::wstring &name);
		MethodCoverage(const MethodCoverage&) = default;
		
		std::wstring GetMethodName() const;
        LineCoverage &AddLine(unsigned int lineNumber, bool hasBeenExecuted);
        void UpdateLine(unsigned int lineNumber, bool hasBeenExecuted);

        const boost::filesystem::path& GetPath() const;
        const LineCoverage* operator[](unsigned int line) const;
        std::vector<LineCoverage> GetLines() const;
		
	private:
        boost::filesystem::path path_;
		std::wstring methodName_;
        std::map<unsigned int, LineCoverage> lines_;
	};
}


