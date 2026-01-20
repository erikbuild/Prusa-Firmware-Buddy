/// @file
#pragma once

#include <variant>
#include <tool_index.hpp>

using ToolMask = std::variant<PhysicalToolIndex, AllTools>;
