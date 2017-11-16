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

#include <Windows.h>
#include "CppCoverageExport.hpp"

namespace CppCoverage
{
	class CPPCOVERAGE_DLL Address
	{
	public:				
		Address(HANDLE hProcess, void* value);
		Address(const Address&) = default;

		HANDLE GetProcessHandle() const;
		void* GetValue() const;

		bool operator<(const Address& other) const;
		friend std::wostream& operator<<(std::wostream&, const Address&);

		Address& operator=(const Address&) = default;

	private:
		HANDLE hProcess_;
		void* value_;
	};
}


