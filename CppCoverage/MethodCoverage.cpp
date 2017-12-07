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
#include "MethodCoverage.hpp"
#include "LineCoverage.hpp"
#include "CppCoverageException.hpp"

namespace CppCoverage
{
	//-------------------------------------------------------------------------
	MethodCoverage::MethodCoverage(const boost::filesystem::path& filePath, const std::wstring &methodName)
		: path_(filePath)
        , methodName_(methodName)
	{
	}
		
    //-------------------------------------------------------------------------
    std::wstring MethodCoverage::GetMethodName() const
    {
        return methodName_;
    }

    //-------------------------------------------------------------------------
    LineCoverage &MethodCoverage::AddLine(unsigned int lineNumber, bool hasBeenExecuted) {
        auto found = lines_.find(lineNumber);
        if (found == lines_.end())
        {
            LineCoverage line{ lineNumber, hasBeenExecuted };
            auto emplace_result = lines_.emplace(lineNumber, line);
            found = emplace_result.first;
        }
        else
            found->second.SetHasBeenExecuted(hasBeenExecuted);
        return found->second;
    }

    //-------------------------------------------------------------------------
    void MethodCoverage::UpdateLine(unsigned int lineNumber, bool hasBeenExecuted) {
        auto found = lines_.find(lineNumber);
        if (found == lines_.end())
            THROW(L"Line " << lineNumber << L" does not exists and cannot be updated for " << path_.wstring());

        found->second.SetHasBeenExecuted(hasBeenExecuted);
    }

    //-------------------------------------------------------------------------
    const boost::filesystem::path& MethodCoverage::GetPath() const {
        return path_;
    }
    //-------------------------------------------------------------------------
    const LineCoverage* MethodCoverage::operator[](unsigned int line) const {
        auto it = lines_.find(line);

        if (it == lines_.end())
            return nullptr;

        return &it->second;
    }

    //-------------------------------------------------------------------------
    std::vector<LineCoverage> MethodCoverage::GetLines() const {
        std::vector<LineCoverage> lines;

        for (const auto& pair : lines_)
            lines.push_back(pair.second);

        return lines;
    }


}
