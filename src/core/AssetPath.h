#pragma once

#include <filesystem>

namespace craft::core {

// Finds one file below the starter's assets directory.
// The helper works from the source tree and from the copied build assets.
[[nodiscard]] std::filesystem::path assetPath(
    const std::filesystem::path& relativePath);

}  // namespace craft::core
