#include "core/AssetPath.h"

#include <sstream>
#include <stdexcept>
#include <vector>

namespace craft::core {

std::filesystem::path assetPath(const std::filesystem::path& relativePath) {
  if (relativePath.empty() || relativePath.is_absolute()) {
    throw std::invalid_argument("Asset paths must be non-empty and relative");
  }

  const std::vector<std::filesystem::path> roots = {
      std::filesystem::current_path() / "assets",
      std::filesystem::path(CRAFT_ASSET_DIR),
  };

  for (const auto& root : roots) {
    const auto candidate = root / relativePath;
    if (std::filesystem::is_regular_file(candidate)) {
      return candidate;
    }
  }

  std::ostringstream message;
  message << "Asset not found: " << relativePath.string() << " (searched";
  for (const auto& root : roots) {
    message << ' ' << root.string();
  }
  message << ')';
  throw std::runtime_error(message.str());
}

}  // namespace craft::core
