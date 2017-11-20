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
#include "ExecutedAddressManager.hpp"

#include <unordered_map>
#include <boost/container/small_vector.hpp>

#include "tools/Log.hpp"

#include "CppCoverageException.hpp"
#include "ModuleCoverage.hpp"
#include "FileCoverage.hpp"
#include "Address.hpp"
#include "SourceCodeLocation.hpp"
#include <iostream>
namespace CppCoverage
{
    //-------------------------------------------------------------------------
    struct ExecutedAddressManager::Line
    {
        explicit Line(unsigned char instructionToRestore, void* dllBaseOfImage)
            : instructionToRestore_{ instructionToRestore }
            , dllBaseOfImage_{ dllBaseOfImage }
        {
        }

        const unsigned char instructionToRestore_;
        void* const dllBaseOfImage_;
        boost::container::small_vector<bool*, 1> hasBeenExecutedCollection_;
    };

    //-------------------------------------------------------------------------
    struct ExecutedAddressManager::Method
    {
        typedef std::list<bool> ConditionalsContainer;
        std::map<unsigned int, ConditionalsContainer> lines_;
//        std::unordered_map<unsigned int, ExecutedAddressManager::Line> methods_;
    };

    //-------------------------------------------------------------------------
    struct ExecutedAddressManager::Class
    {
        std::unordered_map<boost::flyweight<std::wstring>, ExecutedAddressManager::Method> methods_;
    };

    //-------------------------------------------------------------------------
    struct ExecutedAddressManager::File
    {
        const boost::flyweight<std::wstring> name_;
        // Use map to have iterator always valid
        std::map<unsigned int, bool> lines_;
        std::unordered_map<boost::flyweight<std::wstring>, Class> classes_;
    };

	//-------------------------------------------------------------------------
	struct ExecutedAddressManager::Module
	{
		explicit Module(const std::wstring& name) : name_{ name }
		{
		}

		const std::wstring name_;
		std::unordered_map<boost::flyweight<std::wstring>, File> files_;
	};
	
	//-------------------------------------------------------------------------
	ExecutedAddressManager::ExecutedAddressManager()
	{
		lastModule_.baseOfImage_ = nullptr;
		lastModule_.module_ = nullptr;
	}

	//-------------------------------------------------------------------------
	ExecutedAddressManager::~ExecutedAddressManager()
	{
	}

	//-------------------------------------------------------------------------
	void ExecutedAddressManager::AddModule(
		const std::wstring& moduleName,
		void* dllBaseOfImage)
	{
		auto it = modules_.find(moduleName);

		if (it == modules_.end())
			it = modules_.emplace(moduleName, Module{ moduleName }).first;
		lastModule_.module_ = &it->second;
		lastModule_.baseOfImage_ = dllBaseOfImage;
	}
	
	//-------------------------------------------------------------------------
	bool ExecutedAddressManager::RegisterAddress(
		const SourceCodeLocation& location,
		unsigned char instructionValue)
	{
		auto& module = GetLastAddedModule();
		auto& file = module.files_[location.fileName_];
        auto& classContext = file.classes_[location.className_];
        auto& function = classContext.methods_[location.functionName_];
        auto& currentLine = file.lines_[location.lineNumber_];
        function.lines_.emplace(std::make_pair(location.lineNumber_, ExecutedAddressManager::Method::ConditionalsContainer{}));
		LOG_TRACE << "RegisterAddress: " << location.address_ << " for " << location.fileName_ << ":" << location.lineNumber_;
		// Different {filename, line} can have the same address.
		// Same {filename, line} can have several addresses.		
		bool keepBreakpoint = false;
		auto itAddress = addressLineMap_.find(location.address_);

		if (itAddress == addressLineMap_.end())
		{
			itAddress = addressLineMap_.emplace(location.address_, Line{ instructionValue, lastModule_.baseOfImage_ }).first;
			keepBreakpoint = true;
		}
		
		auto& line = itAddress->second;
		line.hasBeenExecutedCollection_.push_back(&currentLine);
        return keepBreakpoint;
	}
    bool ExecutedAddressManager::RegisterBranchAddress(        
        const SourceCodeLocation& location,
        const Address &lineAddress,
        unsigned char instructionValue)
    {
        auto& module = GetLastAddedModule();
        auto& file = module.files_[location.fileName_];
        auto& classContext = file.classes_[location.className_];
        auto& function = classContext.methods_[location.functionName_];
        auto functionLineConditionalContainerItr = function.lines_.find(location.lineNumber_);       
        if (functionLineConditionalContainerItr == function.lines_.end())
        {
            THROW("Conditional for unregistered line encountered");
        }
        LOG_TRACE << "RegisterBranchAddress: " << lineAddress << " for " << location.fileName_ << ":" << location.lineNumber_;
        // Different {filename, line} can have the same address.
        // Same {filename, line} can have several addresses.		
        bool keepBreakpoint = false;
        auto itAddress = addressLineMap_.find(lineAddress);

        if (itAddress == addressLineMap_.end())
        {
            itAddress = addressLineMap_.emplace(lineAddress, Line{ instructionValue, lastModule_.baseOfImage_ }).first;
            keepBreakpoint = true;
        }

        auto& line = itAddress->second;
        functionLineConditionalContainerItr->second.push_front(false);
        line.hasBeenExecutedCollection_.push_back(&functionLineConditionalContainerItr->second.front());
        return keepBreakpoint;
    }
	//-------------------------------------------------------------------------
	ExecutedAddressManager::Module& ExecutedAddressManager::GetLastAddedModule()
	{
		if (!lastModule_.module_)
			THROW("Cannot get last module.");

		return *lastModule_.module_;
	}

	//-------------------------------------------------------------------------
	boost::optional<unsigned char> ExecutedAddressManager::MarkAddressAsExecuted(
		const Address& address)
	{
		auto it = addressLineMap_.find(address);

		if (it == addressLineMap_.end())
			return boost::none;

		auto& line = it->second;

		for (bool* hasBeenExecuted : line.hasBeenExecutedCollection_)
		{
			if (!hasBeenExecuted)
				THROW("Invalid pointer");
			*hasBeenExecuted = true;
		}
		return line.instructionToRestore_;
	}
	
	//-------------------------------------------------------------------------
	CoverageData ExecutedAddressManager::CreateCoverageData(
		const std::wstring& name,
		int exitCode) const
	{
		CoverageData coverageData{ name, exitCode };

		for (const auto& pair : modules_)
		{
			const auto& module = pair.second;
			auto& moduleCoverage = coverageData.AddModule(module.name_);

			for (const auto& file : module.files_)
			{
				const std::wstring& name = file.first;
				const File& fileData = file.second;

				auto& fileCoverage = moduleCoverage.AddFile(name);

				for (const auto& pair : fileData.lines_)
				{
					auto lineNumber = pair.first;
					bool hasLineBeenExecuted = pair.second;
					
					fileCoverage.AddLine(lineNumber, hasLineBeenExecuted);
				}
			}			
		}

		return coverageData;
	}

	//-------------------------------------------------------------------------
	template <typename Condition>
	void ExecutedAddressManager::RemoveAddressLineIf(Condition condition)
	{
		auto it = addressLineMap_.begin();

		while (it != addressLineMap_.end())
		{
			if (condition(*it))
				it = addressLineMap_.erase(it);
			else
				++it;
		}
	}

	//-------------------------------------------------------------------------
	void ExecutedAddressManager::OnExitProcess(HANDLE hProcess)
	{
		RemoveAddressLineIf([=](const auto& pair)
		{
			return pair.first.GetProcessHandle() == hProcess;
		});
	}

	//-------------------------------------------------------------------------
	void ExecutedAddressManager::OnUnloadModule(HANDLE hProcess, void* dllBaseOfImage)
	{
		RemoveAddressLineIf([=](const auto& pair)
		{
			return pair.first.GetProcessHandle() == hProcess
				&& pair.second.dllBaseOfImage_ == dllBaseOfImage;
		});
	}
}
