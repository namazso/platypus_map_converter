//	platypus_map_converter - A map converter for the 2002 game Platypus. 
//	Copyright (C) 2022  namazso <admin@namazso.eu>
//	
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//	
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//	
//	You should have received a copy of the GNU General Public License
//	along with this program.  If not, see <https://www.gnu.org/licenses/>.

#define _CRT_SECURE_NO_WARNINGS
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <vector>
#include <list>
#include <unordered_map>
#include "tiny-json/tiny-json.h"

#ifndef _WIN32
#define _stricmp strcasecmp
#endif

class jsonParser : jsonPool_t
{
  static json_t* alloc_fn(jsonPool_t* pool)
  {
    const auto list_pool = (jsonParser*)pool;
    list_pool->_list.emplace_back();
    return &list_pool->_list.back();
  }

  std::list<json_t> _list{};
  std::string _str{};
  json_t const* _root{};

public:
  jsonParser() : jsonPool_t{ &alloc_fn, &alloc_fn } {}
  jsonParser(const char* str)
    : jsonPool_t{ &alloc_fn, &alloc_fn }
    , _str{ str }
    , _root(json_createWithPool(_str.data(), this)) { }
  ~jsonParser() = default;
  jsonParser(const jsonParser&) = delete;
  jsonParser(jsonParser&&) = delete;
  jsonParser& operator=(const jsonParser&) = delete;
  jsonParser& operator=(jsonParser&&) = delete;

  void parse(const char* str)
  {
    _str = str;
    _list.clear();
    _root = json_createWithPool(_str.data(), this);
  }

  json_t const* root() const { return _root; }
};

static std::vector<uint8_t> read_all(const char* path)
{
  std::ifstream is(path, std::ios::binary);
  if (is.bad() || !is.is_open())
    return {};
  is.seekg(0, std::ifstream::end);
  std::vector<uint8_t> data;
  data.resize((size_t)is.tellg());
  is.seekg(0, std::ifstream::beg);
  is.read(reinterpret_cast<char*>(data.data()), (std::streamsize)data.size());
  return data;
}

void write_all(const char* path, const void* data, size_t size)
{
  std::ofstream os(path, std::ios::binary);
  os.write((const char*)data, size);
  os.close();
}

struct OpCode
{
  uint32_t code;
  std::string name;
  std::vector<std::string> args;
};

OpCode g_opcodes[] =
{
  {0x01, "randSaucers", {"saucers", "firing_chance"}},
  {0x02, "setBonusCounter", {"bonus", "value"}},
  {0x03, "setBonusStar", {"bonus", "type"}},
  {0x04, "redSaucer1", {"bonus", "y", "x"}},
  {0x05, "redBigUn", {"y", "_unknown"}},
  {0x06, "greenBigUn", {"y", "_unknown"}},
  {0x07, "sayLevel"},
  {0x08, "sayArea"},
  {0x09, "destroyText"},
  {0x0A, "blackFish", {"bonus", "x", "y", "script"}},
  {0x0B, "orangeFish", {"bonus", "x", "y", "script"}},
  {0x0C, "skipIfOne", {"places"}},
  {0x0D, "forceBackground", {"layer", "tile"}},
  {0x0F, "toggle_firing", {"disabled"}},
  {0x10, "sayPrims"},
  {0x11, "saySecs"},
  {0x12, "sayTotal"},
  {0x14, "sayEvaluation"},
  {0x15, "sayBonus"},
  {0x16, "giveBonus"},
  {0x17, "setHighRand", {"value"}},
  {0x18, "setLowRand", {"value"}},
  {0x19, "countBonus"},
  {0x1A, "returnCounter1"},
  {0x1B, "returnCounter2"},
  {0x1C, "setLowFront", {"value"}},
  {0x1D, "setHighFront", {"value"}},
  {0x1E, "saucer", {"x", "y", "firing_chance"}},
  {0x21, "make_birds", {"number"}}, // !
  {0x22, "back_redBigUn", {"y"}},
  {0x23, "pylons"},
  {0x24, "back_greenBigUn", {"y"}},
  {0x25, "randFish", {"firing_chance"}},
  {0x26, "squid", {"x", "y", "focus_y", "_unknown"}},
  {0x27, "gunship", {"y"}},
  {0x28, "bigSquid", {"x", "y", "focus_y", "_unknown"}},
  {0x29, "ray"},
  {0x2A, "setFishFire", {"enabled"}}, // !
  {0x2B, "blimp", {"y", "message"}},
  {0x2C, "special_for_no_explode"},
  {0x2D, "skip_if_no_special", {"places"}},
  {0x2E, "balloon", {"x", "y", "bonus_type"}},
  {0x2F, "moon", {"x", "y", "x_velocity", "y_velocity", "sprite", "in_front"}},
  {0x30, "tanks_n_building", {"tanks", "item"}},
  {0x31, "goldfish", {"x", "y", "_unknown"}},
  {0x32, "wait_for_no_enemies"},
  {0x33, "make_waterfall", {"layer"}},
  {0x34, "_unknown34", {"_unknown"}}, // not used in map
  {0x35, "make_mounted_guns", {"pattern"}},
  {0x36, "set_hillpop", {"layer"}},
  {0x37, "wait_hillpop", {"layer"}},
  {0x38, "make_train", {"carriage"}},
  {0x39, "load_cars"},
  {0x3A, "unload_cars"},
  {0x3C, "say_level_over"},
  {0x3D, "say_bonus_credit"},
  {0x3E, "give_bonus_credit"},
  {0x3F, "bomb", {"x", "y", "x_velocity", "y_velocity", "string_length", "deadly", "bonus"}},
  {0x40, "set_ray", {"x", "y", "direction", "_unknown"}},
  {0x41, "block_layer", {"layer"}}, // not used in map
  {0x42, "unblock_layer", {"layer"}},
  {0x43, "distant_boat", {"y"}},
  {0x44, "distant_island", {"y"}},
  {0x45, "homing_ship", {"y"}},
  {0x46, "back_bomb", {"x", "y", "x_velocity", "y_velocity", "layer"}},
  {0x47, "flip_plane", {"y"}},
  {0x48, "bigPlane"},
  {0x49, "back_boss2"},
  {0x4A, "eruption"},
  {0x4B, "parrots", {"number"}},
  {0x4C, "fast_ship", {"bonus"}},
  {0x4E, "missile_boat"},
  {0x4F, "cannon_boat"},
  {0x50, "cargo_boat", {"_unknown"}},
  {0x51, "_unknown51", {"x", "y", "focus_y", "_unknown"}}, // not used in map
  {0x52, "sub"},
  {0x53, "laser_squid", {"x", "y", "focus_y", "_unknown"}},
  {0x54, "last_boss"},
  {0x55, "say_bonus_250000"},
  {0x56, "bonus_credits"},
  {0x57, "give_end_bonus"},
  {0x58, "convert_counter"},
  {0x59, "say_mission_over"},
  {0x5A, "returncounter3"},
  {0x5B, "kill_game"},
  {0x5C, "the_end"},
  {0x5D, "music_fade"},
  {0x5E, "baddie_tune", {"tune"}},
  {0x64, "nufzenuf"},
  {0x151F04BD, "noEruption"},
  {0x23A15D71, "end_level"},
  {0x9E051FB8, "next_level"},
};

constexpr static auto k_terminator = 0x2A6FE0EF;

std::unordered_map<int32_t, const OpCode*> g_from_code = []
{
  std::unordered_map<int32_t, const OpCode*> map;
  for (const auto& v : g_opcodes)
    map[v.code] = &v;
  return map;
} ();

std::unordered_map<std::string, const OpCode*> g_from_name = []
{
  std::unordered_map<std::string, const OpCode*> map;
  for (const auto& v : g_opcodes)
    map[v.name] = &v;
  return map;
} ();

int compile(const std::vector<uint8_t>& file, FILE* ofile)
{
  jsonParser json{std::string{(char*)file.data(), (char*)file.data() + file.size()}.c_str()};

  const auto root = json.root();
  if (!root)
    return fprintf(stderr, "Invalid json (parsing failed)\n"), -2;
  if (json_getType(root) != JSON_ARRAY)
    return fprintf(stderr, "Invalid json (root not array)\n"), -3;
  size_t i = 0;
  for (auto it = json_getChild(root); it != nullptr; ++i, it = json_getSibling(it))
  {
    if (json_getType(it) != JSON_OBJ)
      return fprintf(stderr, "Invalid json (array member not object)\n"), -4;
    const auto wait_prop = json_getProperty(it, "wait");
    if (wait_prop && json_getType(wait_prop) != JSON_INTEGER)
      return fprintf(stderr, "Invalid json (wait not integer in #%zu)\n", i), -5;
    const auto wait = wait_prop ? (int32_t)json_getInteger(wait_prop) : 0;
    const auto action = json_getPropertyValue(it, "action");
    if (!action)
      return fprintf(stderr, "Invalid json (no action in object #%zu)\n", i), -6;
    const auto op_it = g_from_name.find(action);
    if (op_it == g_from_name.end())
      return fprintf(stderr, "Invalid json (unknown action %s in #%zu)\n", action, i), -7;
    const auto op = *op_it->second;
    std::vector<int32_t> args;
    if (!op.args.empty())
    {
      const auto args_prop = json_getProperty(it, "args");
      if (!args_prop)
        return fprintf(stderr, "Invalid json (args missing for %s in #%zu)\n", action, i), -8;
      if (json_getType(args_prop) != JSON_OBJ)
        return fprintf(stderr, "Invalid json (args not object for %s in #%zu)\n", action, i), -9;
      for (const auto& arg : op.args)
      {
        const auto arg_prop = json_getProperty(args_prop, arg.c_str());
        if (!arg_prop)
          return fprintf(stderr, "Invalid json (arg %s missing for %s in #%zu)\n", arg.c_str(), action, i), -10;
        if (json_getType(arg_prop) != JSON_INTEGER)
          return fprintf(stderr, "Invalid json (arg %s not integer for %s in #%zu)\n", arg.c_str(), action, i), -11;
        args.push_back((int32_t)json_getInteger(arg_prop));
      }
    }
    fwrite(&wait, sizeof(int32_t), 1, ofile);
    fwrite(&op.code, sizeof(uint32_t), 1, ofile);
    fwrite(args.data(), sizeof(int32_t), args.size(), ofile);
  }
  fwrite(&k_terminator, sizeof(uint32_t), 1, ofile);
  return 0;
}

int decompile(const std::vector<uint8_t>& file, FILE* ofile)
{
  if (file.size() < 4 || file.size() % 4 != 0)
    return fprintf(stderr, "Invalid mapfile\n"), -2;
  const auto begin = (int32_t*)file.data();
  auto end = (int32_t*)(file.data() + file.size());
  if (end[-1] != k_terminator)
    return fprintf(stderr, "Invalid mapfile (no end marker)\n"), -3;
  --end;
  fprintf(ofile, "[\n");
  for (auto it = begin; it != end; ++it)
  {
    if (it != begin)
      fprintf(ofile, ",\n");
    const auto wait = *it;
    ++it;
    if (it == end)
      return fprintf(stderr, "Unexpected end of file\n"), -4;
    const auto opcode = *it;
    const auto op_it = g_from_code.find(opcode);
    if (op_it == g_from_code.end())
      return fprintf(stderr, "Invalid opcode: %08X\n", opcode), -5;
    const auto& op = *op_it->second;
    const auto args_count = op.args.size();
    if (it + args_count >= end)
      return fprintf(stderr, "Unexpected end of file\n"), -6;
    const auto args = it + 1;

    fprintf(ofile, "{");
    if (wait)
      fprintf(ofile, R"("wait": %d, )", wait);
    fprintf(ofile, R"("action": "%s")", op.name.c_str());

    if (args_count)
    {
      it += args_count;
      fprintf(ofile, R"(, "args": {)");
      for (size_t i = 0; i < args_count; ++i)
        fprintf(ofile, R"("%s": %d%s)", op.args[i].c_str(), args[i], i == args_count - 1 ? "}" : ", ");
    }
    fprintf(ofile, "}");
  }
  fprintf(ofile, "\n]\n");
  return 0;
}

int main(int argc, char** argv)
{
  auto invocation = strrchr(argv[0], '\\');
  invocation = invocation ? invocation + 1 : argv[0];
  bool is_compile{};
  if (argc < 4 || (!(is_compile = 0 == _stricmp(argv[1], "compile")) && 0 != _stricmp(argv[1], "decompile")))
    return fprintf(stderr, "Usage: %s [compile|decompile] [INPUT] [OUTPUT]\n"
      R"(
Copyright (C) 2022  namazso <admin@namazso.eu>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

This software includes tiny-json:

Copyright (c) 2018 Rafa Garcia

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
)", invocation), -1;
  const auto file = read_all(argv[2]);
  FILE* ofile;
  if (0 == strcmp(argv[3], "-"))
    ofile = stdout;
  else if (ofile = fopen(argv[3], "wb"); !ofile)
    return fprintf(stderr, "Cannot open output file: %s\n", strerror(errno)), -100;
  return is_compile ? compile(file, ofile) : decompile(file, ofile);
}