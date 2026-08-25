#pragma once

#include "Core/Path/FilesystemPath.h"
#include <QString>
#include <utility>

namespace Domain::Game {

enum class SearchTargetType {
    Directory,
    Vpk
};

class SearchTarget {
public:
    SearchTarget() = default;
    SearchTarget(SearchTargetType type, Core::Path::FilesystemPath path)
        : m_type(type), m_path(std::move(path)) {}

    static SearchTarget makeDirectory(Core::Path::FilesystemPath path) {
        return SearchTarget(SearchTargetType::Directory, std::move(path));
    }

    static SearchTarget makeVpk(Core::Path::FilesystemPath path) {
        return SearchTarget(SearchTargetType::Vpk, std::move(path));
    }

    SearchTargetType type() const noexcept { return m_type; }
    void setType(SearchTargetType type) noexcept { m_type = type; }

    bool isVpk() const noexcept { return m_type == SearchTargetType::Vpk; }
    bool isDirectory() const noexcept { return m_type == SearchTargetType::Directory; }

    const Core::Path::FilesystemPath& path() const noexcept { return m_path; }
    void setPath(Core::Path::FilesystemPath path) { m_path = std::move(path); }

    QString pathString() const { return m_path.toString(); }

    bool operator==(const SearchTarget& other) const noexcept {
        return m_type == other.m_type && m_path == other.m_path;
    }

    bool operator!=(const SearchTarget& other) const noexcept {
        return !(*this == other);
    }

private:
    SearchTargetType m_type = SearchTargetType::Directory;
    Core::Path::FilesystemPath m_path;
};

} // namespace Domain::Game

