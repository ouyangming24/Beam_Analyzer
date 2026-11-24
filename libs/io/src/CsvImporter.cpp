#include "beam/io/CsvImporter.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace beam::io {
namespace {

std::string trim(std::string s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
    s.erase(s.begin());
  }
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
    s.pop_back();
  }
  return s;
}

std::vector<std::string> split(const std::string& line, char delim) {
  std::vector<std::string> parts;
  std::stringstream ss(line);
  std::string item;
  while (std::getline(ss, item, delim)) {
    parts.push_back(trim(item));
  }
  // Preserve trailing empty field if line ends with delimiter
  if (!line.empty() && line.back() == delim) {
    parts.emplace_back();
  }
  return parts;
}

bool parseMetaLine(const std::string& line, CsvImportOptions& options) {
  // Examples:
  // # pixel_um=5.0
  // # pixel_um_x=5.2,pixel_um_y=5.0
  if (line.empty() || line.front() != '#') {
    return false;
  }
  const auto body = trim(line.substr(1));
  auto fields = split(body, ',');
  for (const auto& f : fields) {
    const auto eq = f.find('=');
    if (eq == std::string::npos) {
      continue;
    }
    const auto key = trim(f.substr(0, eq));
    const auto val = trim(f.substr(eq + 1));
    try {
      const double v = std::stod(val);
      if (key == "pixel_um" || key == "pixel_size_um") {
        options.pixelSizeX_um = v;
        options.pixelSizeY_um = v;
      } else if (key == "pixel_um_x" || key == "pixel_size_x_um") {
        options.pixelSizeX_um = v;
      } else if (key == "pixel_um_y" || key == "pixel_size_y_um") {
        options.pixelSizeY_um = v;
      }
    } catch (...) {
      // ignore malformed meta fields
    }
  }
  return true;
}

}  // namespace

CsvImportResult loadFrameCsv(const std::string& path, const CsvImportOptions& optionsIn) {
  CsvImportResult result;
  CsvImportOptions options = optionsIn;

  std::ifstream in(path);
  if (!in) {
    result.message = "Cannot open file: " + path;
    return result;
  }

  std::vector<std::vector<double>> rows;
  std::string line;
  bool first = true;

  while (std::getline(in, line)) {
    line = trim(line);
    if (line.empty()) {
      continue;
    }
    if (first && line.front() == '#') {
      parseMetaLine(line, options);
      first = false;
      continue;
    }
    first = false;

    // Skip optional textual header row
    if (options.hasHeader) {
      options.hasHeader = false;
      bool allNumeric = true;
      for (const auto& tok : split(line, options.delimiter)) {
        if (tok.empty()) {
          continue;
        }
        try {
          (void)std::stod(tok);
        } catch (...) {
          allNumeric = false;
          break;
        }
      }
      if (!allNumeric) {
        continue;
      }
    }

    auto tokens = split(line, options.delimiter);
    std::vector<double> row;
    row.reserve(tokens.size());
    for (const auto& tok : tokens) {
      if (tok.empty()) {
        continue;
      }
      try {
        row.push_back(std::stod(tok));
      } catch (...) {
        result.message = "Non-numeric value in CSV: '" + tok + "'";
        return result;
      }
    }
    if (!row.empty()) {
      rows.push_back(std::move(row));
    }
  }

  if (rows.empty()) {
    result.message = "CSV contains no numeric rows";
    return result;
  }

  const std::size_t width = rows.front().size();
  for (const auto& r : rows) {
    if (r.size() != width) {
      result.message = "CSV rows have inconsistent column counts";
      return result;
    }
  }

  result.frame.resize(width, rows.size());
  result.frame.pixelSizeX_um = options.pixelSizeX_um;
  result.frame.pixelSizeY_um = options.pixelSizeY_um;
  for (std::size_t y = 0; y < rows.size(); ++y) {
    for (std::size_t x = 0; x < width; ++x) {
      result.frame.intensity[y * width + x] = rows[y][x];
    }
  }

  result.ok = true;
  result.message = "OK";
  return result;
}

}  // namespace beam::io
