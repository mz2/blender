/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2021 by Blender Foundation.
 * All rights reserved.
 */

#pragma once

#include "shader_compiler.hh"

#include "shaderc/shaderc.hpp"

namespace shader_compiler::shaderc {

class ShaderCResult : public Result {

 public:
  void init(const Job &job, ::shaderc::SpvCompilationResult &shaderc_result);

 private:
  StatusCode status_code_from(::shaderc::SpvCompilationResult &shaderc_result);
  std::string error_log_from(::shaderc::SpvCompilationResult &shaderc_result);
  std::vector<uint32_t> bin_from(::shaderc::SpvCompilationResult &shaderc_result);
};

}  // namespace shader_compiler::shaderc