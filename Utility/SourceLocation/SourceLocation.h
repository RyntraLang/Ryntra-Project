// ========== SourceLocation.h =============================== *- C++ -* ========== //
// Copyright (c) 2026 Remimwen Studio. All rights reserved.
// Part of the Ryntra Project. Licensed under Apache-2.0 License.
// ================================================================================ //

#pragma once
#include <cstdint>

/**
 * @brief Opt-in deprecation annotations for the point-based @c SourceLocation API.
 * Disabled by default so the build stays warning-clean while call sites migrate to
 * @c SourceRange . Define @c RYNTRA_ENABLE_LOCATION_DEPRECATION on the top of the
 * header file or the build script to turn them on.
 */
#if defined(RYNTRA_ENABLE_LOCATION_DEPRECATION)
#  define RYNTRA_DEPRECATED_LOCATION \
       [[deprecated("SourceRange is deprecated. Use getRange()/setRange() instead.")]]
#  define RYNTRA_DEPRECATED_LOCATION_OVERLOAD \
       [[deprecated("SourceRange is deprecated. Use getRange()/setRange() instead.")]]
#else
#  define RYNTRA_DEPRECATED_LOCATION
#  define RYNTRA_DEPRECATED_LOCATION_OVERLOAD
#endif

namespace Ryntra::Compiler {
    /**
     * @brief A simple structure represents the location in the source file.
     */
    struct SourceLocation {
        uint32_t line;
        uint32_t column;
        uint32_t offset;

        /**
         * @brief Constructor. Use default behavior.
         */
        SourceLocation() = default;

        /**
         * @brief Constructor. Accepts the @c line, @c column and the @c offset .
         * @param _line Current line
         * @param _column Current column
         * @param offs The offset from the start of the file.
         */
        SourceLocation(const uint32_t _line, const uint32_t _column, const uint32_t offs) : line(_line), column(_column), offset(offs) {}
    };
} // namespace Ryntra::Compiler