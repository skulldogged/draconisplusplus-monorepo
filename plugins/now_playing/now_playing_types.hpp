/**
 * @file now_playing_types.hpp
 * @brief Shared types for the Now Playing plugin
 */

#pragma once

#include <Drac++/Utils/Types.hpp>

namespace now_playing {
  using namespace draconis::utils::types;

  /**
   * @brief Media information data structure
   */
  struct MediaData {
    Option<String> title;
    Option<String> artist;
    Option<String> album;
    Option<String> playerName;
  };

  /**
   * @brief Plugin configuration
   */
  struct NowPlayingConfig {
    bool enabled = true;
  };
} // namespace now_playing
