#include "Inline/BasicTypes.h"
#include "Runtime.h"
#include "RuntimePrivate.h"
#include "IR/Module.h"

#include <string.h>

namespace Runtime
{
	std::vector<ModuleInstance*> moduleInstances;
	
	Value evaluateInitializer(ModuleInstance* moduleInstance,InitializerExpression expression)
	{
		switch(expression.type)
		{
		case InitializerExpression::Type::i32_const: return expression.i32;
		case InitializerExpression::Type::i64_const: return expression.i64;
		case InitializerExpression::Type::f32_const: return expression.f32;
		case InitializerExpression::Type::f64_const: return expression.f64;
		case InitializerExpression::Type::get_global:
		{
			// Find the import this refers to.
			errorUnless(expression.globalIndex < moduleInstance->globals.size());
			GlobalInstance* globalInstance = moduleInstance->globals[expression.globalIndex];
			return Runtime::Value(globalInstance->type.valueType,globalInstance->value);
		}
		default: Errors::unreachable();
		};
	}

	MemoryInstance* theMemoryInstance = nullptr;

	ModuleInstance* instantiateModule(const IR::Module& module,ImportBindings&& imports)
	{
		ModuleInstance* moduleInstance = new ModuleInstance(
			std::move(imports.functions),
			std::move(imports.tables),
			std::move(imports.memories),
			std::move(imports.globals)
			);
		
		// Get disassembly names for the module's objects.
		DisassemblyNames disassemblyNames;
		IR::getDisassemblyNames(module,disassemblyNames);

		// Check the type of the ModuleInstance's imports.
		errorUnless(moduleInstance->functions.size() == module.functions.imports.size());
		for(Uptr importIndex = 0;importIndex < module.functions.imports.size();++importIndex)
		{
			errorUnless(isA(moduleInstance->functions[importIndex],module.types[module.functions.imports[importIndex].type.index]));
		}
		errorUnless(moduleInstance->tables.size() == module.tables.imports.size());
		for(Uptr importIndex = 0;importIndex < module.tables.imports.size();++importIndex)
		{
			errorUnless(isA(moduleInstance->tables[importIndex],module.tables.imports[importIndex].type));
		}
		errorUnless(moduleInstance->memories.size() == module.memories.imports.size());
		for(Uptr importIndex = 0;importIndex < module.memories.imports.size();++importIndex)
		{
			errorUnless(isA(moduleInstance->memories[importIndex],module.memories.imports[importIndex].type));
		}
		errorUnless(moduleInstance->globals.size() == module.globals.imports.size());
		for(Uptr importIndex = 0;importIndex < module.globals.imports.size();++importIndex)
		{
			errorUnless(isA(moduleInstance->globals[importIndex],module.globals.imports[importIndex].type));
		}

		// Instantiate the module's memory and table definitions.
		for(const TableDef& tableDef : module.tables.defs)
		{
			auto table = createTable(tableDef.type);
			if(!table) { causeException(Exception::Cause::outOfMemory); }
			moduleInstance->tables.push_back(table);
		}
		for(const MemoryDef& memoryDef : module.memories.defs)
		{
			if(!theMemoryInstance) {
				theMemoryInstance = createMemory(memoryDef.type);
				if(!theMemoryInstance) { causeException(Exception::Cause::outOfMemory); }
			}
			moduleInstance->memories.push_back(theMemoryInstance);
		}

		// Find the default memory and table for the module.
		if(moduleInstance->memories.size() != 0)
		{
			WAVM_ASSERT_THROW(moduleInstance->memories.size() == 1);
			moduleInstance->defaultMemory = moduleInstance->memories[0];
		}
		if(moduleInstance->tables.size() != 0)
		{
			WAVM_ASSERT_THROW(moduleInstance->tables.size() == 1);
			moduleInstance->defaultTable = moduleInstance->tables[0];
		}

		// If any memory or table segment doesn't fit, throw an exception before mutating any memory/table.
		for(auto& tableSegment : module.tableSegments)
		{
			TableInstance* table = moduleInstance->tables[tableSegment.tableIndex];
			const Value baseOffsetValue = evaluateInitializer(moduleInstance,tableSegment.baseOffset);
			errorUnless(baseOffsetValue.type == ValueType::i32);
			const U32 baseOffset = baseOffsetValue.i32;
			if(baseOffset > table->elements.size()
			|| table->elements.size() - baseOffset < tableSegment.indices.size())
			{ causeException(Exception::Cause::invalidSegmentOffset); }
		}

		//Previously, the module instantiation would write in to the memoryInstance here. Don't do that
      //since the memoryInstance is shared across all moduleInstances and we could be compiling
      //a new instance while another instance is running
		
		// Instantiate the module's global definitions.
		for(const GlobalDef& globalDef : module.globals.defs)
		{
			const Value initialValue = evaluateInitializer(moduleInstance,globalDef.initializer);
			errorUnless(initialValue.type == globalDef.type.valueType);
			moduleInstance->globals.push_back(new GlobalInstance(globalDef.type,initialValue));
		}
		
		// Create the FunctionInstance objects for the module's function definitions.
		for(Uptr functionDefIndex = 0;functionDefIndex < module.functions.defs.size();++functionDefIndex)
		{
			const Uptr functionIndex = moduleInstance->functions.size();
			const DisassemblyNames::Function& functionNames = disassemblyNames.functions[functionIndex];
			std::string debugName = functionNames.name;
			if(!debugName.size()) { debugName = "<function #" + std::to_string(functionDefIndex) + ">"; }
			auto functionInstance = new FunctionInstance(moduleInstance,module.types[module.functions.defs[functionDefIndex].type.index],nullptr,debugName.c_str());
			moduleInstance->functionDefs.push_back(functionInstance);
			moduleInstance->functions.push_back(functionInstance);
		}

		// Generate machine code for the module.
		LLVMJIT::instantiateModule(module,moduleInstance);

		// Set up the instance's exports.
		for(const Export& exportIt : module.exports)
		{
			ObjectInstance* exportedObject = nullptr;
			switch(exportIt.kind)
			{
			case ObjectKind::function: exportedObject = moduleInstance->functions[exportIt.index]; break;
			case ObjectKind::table: exportedObject = moduleInstance->tables[exportIt.index]; break;
			case ObjectKind::memory: exportedObject = moduleInstance->memories[exportIt.index]; break;
			case ObjectKind::global: exportedObject = moduleInstance->globals[exportIt.index]; break;
			default: Errors::unreachable();
			}
			moduleInstance->exportMap[exportIt.name] = exportedObject;
		}
		
		// Copy the module's table segments into the module's default table.
		for(const TableSegment& tableSegment : module.tableSegments)
		{
			TableInstance* table = moduleInstance->tables[tableSegment.tableIndex];
			
			const Value baseOffsetValue = evaluateInitializer(moduleInstance,tableSegment.baseOffset);
			errorUnless(baseOffsetValue.type == ValueType::i32);
			const U32 baseOffset = baseOffsetValue.i32;
			WAVM_ASSERT_THROW(baseOffset + tableSegment.indices.size() <= table->elements.size());

			for(Uptr index = 0;index < tableSegment.indices.size();++index)
			{
				const Uptr functionIndex = tableSegment.indices[index];
				WAVM_ASSERT_THROW(functionIndex < moduleInstance->functions.size());
				setTableElement(table,baseOffset + index,moduleInstance->functions[functionIndex]);
			}
		}

		// Call the module's start function.
		if(module.startFunctionIndex != UINTPTR_MAX)
		{
			WAVM_ASSERT_THROW(moduleInstance->functions[module.startFunctionIndex]->type == IR::FunctionType::get());
			moduleInstance->startFunctionIndex = module.startFunctionIndex;
		}

		moduleInstances.push_back(moduleInstance);
		return moduleInstance;
	}

	ModuleInstance::~ModuleInstance()
	{
		delete jitModule;
	}

	MemoryInstance* getDefaultMemory(ModuleInstance* moduleInstance) { return moduleInstance->defaultMemory; }
	uint64_t getDefaultMemorySize(ModuleInstance* moduleInstance) { return moduleInstance->defaultMemory->numPages << IR::numBytesPerPageLog2; }
	TableInstance* getDefaultTable(ModuleInstance* moduleInstance) { return moduleInstance->defaultTable; }

	void runInstanceStartFunc(ModuleInstance* moduleInstance) {
		if(moduleInstance->startFunctionIndex != UINTPTR_MAX)
			invokeFunction(moduleInstance->functions[moduleInstance->startFunctionIndex],{});
	}

	void resetGlobalInstances(ModuleInstance* moduleInstance) {
		for(GlobalInstance*& gi : moduleInstance->globals)
			memcpy(&gi->value, &gi->initialValue, sizeof(gi->value));
	}
	
	ObjectInstance* getInstanceExport(ModuleInstance* moduleInstance,const std::string& name)
	{
		auto mapIt = moduleInstance->exportMap.find(name);
		return mapIt == moduleInstance->exportMap.end() ? nullptr : mapIt->second;
	}
}
