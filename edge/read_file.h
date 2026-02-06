#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "..\shared\vpoker.h"

struct FileContents {
  std::string game_name;
  game_kind kind;
  denom_value high;
  std::array<std::uint16_t, static_cast<std::size_t>(last_pay) + 1> pay_table;
};

std::optional<FileContents> read_file(const std::string& filename);
