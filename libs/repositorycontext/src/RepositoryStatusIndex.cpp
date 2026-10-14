// SPDX-FileCopyrightText: 2026 LinuxGitShell contributors
// SPDX-License-Identifier: MIT

#include "linuxgitshell/repositorycontext/RepositoryStatusIndex.h"

#include "linuxgitshell/repositorycontext/RepositoryContextSnapshot.h"

#include <algorithm>
#include <utility>

namespace LinuxGitShell
{
namespace
{

using State = RepositoryStatusState;
using FileState = RepositoryFileState;
using Validity = RepositoryStatusValidity;
using StoreResult = RepositoryStatusStoreResult;

// Includes conservative hash/record overhead; separately account for every stored string.
constexpr qsizetype RecordBytes = 256;

bool validPath(const QString& path)
{
    if (path.isEmpty() || path.contains(QChar::Null))
    {
        return false;
    }
    for (const QStringView component : QStringView(path).split(QLatin1Char('/')))
    {
        if (component.isEmpty() || component == u"." || component == u".." || component == u".git")
        {
            return false;
        }
    }
    return true;
}

bool validFileState(FileState state)
{
    switch (state)
    {
    case FileState::Clean:
    case FileState::Modified:
    case FileState::Added:
    case FileState::Deleted:
    case FileState::Renamed:
    case FileState::Copied:
    case FileState::TypeChanged:
    case FileState::Conflict:
    case FileState::Untracked:
    case FileState::Ignored:
        return true;
    }
    return false;
}

bool validValidity(Validity validity)
{
    switch (validity)
    {
    case Validity::Ready:
    case Validity::Pending:
    case Validity::Stale:
    case Validity::Degraded:
    case Validity::Unavailable:
        return true;
    }
    return false;
}

bool renameOrCopy(FileState state)
{
    return state == FileState::Renamed || state == FileState::Copied;
}

State presentation(FileState state)
{
    switch (state)
    {
    case FileState::Clean:
        return State::Clean;
    case FileState::Modified:
    case FileState::Renamed:
    case FileState::TypeChanged:
        return State::Modified;
    case FileState::Added:
    case FileState::Copied:
        return State::Added;
    case FileState::Deleted:
        return State::Deleted;
    case FileState::Conflict:
        return State::Conflict;
    case FileState::Untracked:
        return State::Untracked;
    case FileState::Ignored:
        return State::Ignored;
    }
    return State::Unknown;
}

State presentation(const RepositoryStatusEntry& entry)
{
    State workTree = presentation(entry.workTreeState);
    // Porcelain marks a submodule's work tree M even when S..U reports only untracked content.
    // Preserve the detailed M while presenting that specific case as Untracked.
    if (entry.isSubmodule && entry.workTreeState == FileState::Modified &&
        entry.submoduleUntrackedChanges && !entry.submoduleCommitChanged &&
        !entry.submoduleTrackedChanges)
    {
        workTree = State::Untracked;
    }
    State state = std::max(presentation(entry.indexState), workTree);
    if (entry.isSubmodule)
    {
        if (entry.submoduleCommitChanged || entry.submoduleTrackedChanges)
        {
            state = std::max(state, State::Modified);
        }
        else if (entry.submoduleUntrackedChanges)
        {
            state = std::max(state, State::Untracked);
        }
    }
    return state;
}

// Visits strict ancestors, ending at the repository root. No filesystem access or recursion.
template <typename Visitor> bool ancestors(const QString& path, Visitor visit)
{
    if (path.isEmpty())
    {
        return true;
    }
    qsizetype end = path.size();
    while (end > 0)
    {
        end = path.lastIndexOf(QLatin1Char('/'), end - 1);
        if (!visit(end < 0 ? QString() : path.left(end)))
        {
            return false;
        }
    }
    return true;
}

} // namespace

RepositoryStatusIndex::RepositoryStatusIndex(const QString& repositoryRoot,
                                             const RepositoryStatusIndexLimits& limits,
                                             const RepositoryStatusIndexOptions& options)
    : root(repositoryContextPathKey(repositoryRoot)), limits(limits), options(options)
{
    if (repositoryRoot.contains(QChar::Null))
    {
        root.clear();
    }
}

bool RepositoryStatusIndex::reserveStorage(qsizetype bytes, qsizetype records)
{
    if (bytes < 0 || records < 0 || bytes > limits.maxAccountedBytes - bytesUsed ||
        records > limits.maxRecords - recordsUsed)
    {
        return false;
    }
    bytesUsed += bytes;
    recordsUsed += records;
    return true;
}

bool RepositoryStatusIndex::ensureNode(const QString& path)
{
    if (nodes.contains(path))
    {
        return true;
    }
    if (!reserveStorage(RecordBytes + (path.size() + 1) * qsizetype(sizeof(QChar)), 1))
    {
        return false;
    }
    nodes.insert(path, Node{});
    return true;
}

bool RepositoryStatusIndex::ensureAncestors(const QString& path)
{
    return ancestors(path,
                     [this](const QString& parent)
                     {
                         if (!ensureNode(parent))
                         {
                             return false;
                         }
                         nodes[parent].directory = true;
                         return true;
                     });
}

bool RepositoryStatusIndex::belowBoundary(const QString& path) const
{
    return !ancestors(path, [this](const QString& parent) { return !boundaries.contains(parent); });
}

StoreResult RepositoryStatusIndex::build(const RepositoryStatusSnapshot& snapshot)
{
    if (root.isEmpty() || !validValidity(snapshot.validity))
    {
        return StoreResult::InvalidSnapshot;
    }
    currentValidity = snapshot.validity;
    if (currentValidity != Validity::Ready)
    {
        return StoreResult::Accepted;
    }
    if (!snapshot.coverageComplete)
    {
        return StoreResult::InvalidSnapshot;
    }
    if (limits.maxRecords <= 0 || limits.maxAccountedBytes <= 0 ||
        snapshot.trackedPaths.size() > limits.maxRecords ||
        snapshot.entries.size() > limits.maxRecords ||
        snapshot.excludedRoots.size() > limits.maxRecords ||
        !reserveStorage((root.size() + 1) * qsizetype(sizeof(QChar))) || !ensureNode(QString()))
    {
        return StoreResult::LimitExceeded;
    }
    nodes[QString()].directory = true;
    for (const QString& path : snapshot.excludedRoots)
    {
        if (!validPath(path))
        {
            return StoreResult::InvalidSnapshot;
        }
        if (!boundaries.contains(path))
        {
            if (!reserveStorage(RecordBytes + (path.size() + 1) * qsizetype(sizeof(QChar)), 1))
            {
                return StoreResult::LimitExceeded;
            }
            boundaries.insert(path);
        }
    }
    for (const QString& path : snapshot.trackedPaths)
    {
        if (!validPath(path))
        {
            return StoreResult::InvalidSnapshot;
        }
        if (belowBoundary(path))
        {
            continue;
        }
        if (!ensureNode(path) || !ensureAncestors(path))
        {
            return StoreResult::LimitExceeded;
        }
        nodes[path].tracked = true;
    }
    for (const RepositoryStatusEntry& entry : snapshot.entries)
    {
        if (!validPath(entry.path) || !validFileState(entry.indexState) ||
            !validFileState(entry.workTreeState) ||
            (!entry.originalPath.isEmpty() && !validPath(entry.originalPath)) ||
            ((renameOrCopy(entry.indexState) || renameOrCopy(entry.workTreeState)) &&
             entry.originalPath.isEmpty()) ||
            (!entry.isSubmodule && (entry.submoduleCommitChanged || entry.submoduleTrackedChanges ||
                                    entry.submoduleUntrackedChanges)))
        {
            return StoreResult::InvalidSnapshot;
        }
        if (belowBoundary(entry.path))
        {
            continue;
        }
        if (!ensureNode(entry.path) || !ensureAncestors(entry.path))
        {
            return StoreResult::LimitExceeded;
        }
        Node& node = nodes[entry.path];
        if (node.entry.has_value())
        {
            return StoreResult::InvalidSnapshot;
        }
        if (!reserveStorage((entry.path.size() + entry.originalPath.size() + 2) *
                            qsizetype(sizeof(QChar))))
        {
            return StoreResult::LimitExceeded;
        }
        node.entry = entry;
        node.directory = node.directory || entry.isDirectory;
    }
    aggregateDirectories();
    return StoreResult::Accepted;
}

StoreResult RepositoryStatusIndex::replace(const RepositoryStatusSnapshot& snapshot)
{
    if (snapshot.revision == 0 || snapshot.revision <= currentRevision)
    {
        return StoreResult::ObsoleteRevision;
    }
    RepositoryStatusIndex next(root, limits, options);
    next.currentRevision = snapshot.revision;
    next.capturedAt = snapshot.capturedAt;
    const StoreResult result = next.build(snapshot);
    if (result != StoreResult::Accepted)
    {
        next.discard(Validity::Degraded);
    }
    *this = std::move(next);
    return result;
}

RepositoryStatusState RepositoryStatusIndex::directState(const QString& path,
                                                         const Node* node) const
{
    if (node != nullptr)
    {
        if (node->entry.has_value())
        {
            return presentation(node->entry.value());
        }
        if (node->tracked)
        {
            return State::Clean;
        }
    }
    State state = State::Unknown;
    (void)ancestors(path,
                    [this, &state](const QString& parent)
                    {
                        const auto it = nodes.constFind(parent);
                        if (it != nodes.cend() && it->entry.has_value() && it->entry->isDirectory &&
                            presentation(it->entry.value()) == State::Ignored)
                        {
                            state = State::Ignored;
                            return false;
                        }
                        return true;
                    });
    return state;
}

void RepositoryStatusIndex::aggregateDirectories()
{
    if (!options.recursive)
    {
        return;
    }
    for (auto it = nodes.cbegin(); it != nodes.cend(); ++it)
    {
        // Only direct evidence contributes; unknown paths are never inferred clean here.
        if (!it->tracked && !it->entry.has_value())
        {
            continue;
        }
        const State state = directState(it.key(), &it.value());
        if (state == State::Unknown || (state == State::Ignored && !options.includeIgnored) ||
            (state == State::Untracked && !options.includeUntracked))
        {
            continue;
        }
        (void)ancestors(it.key(),
                        [this, state](const QString& parent)
                        {
                            Node& node = nodes[parent];
                            node.aggregate = std::max(node.aggregate, state);
                            return true;
                        });
    }
}

RepositoryStatusLookup RepositoryStatusIndex::lookup(const QString& relativePath,
                                                     Clock::time_point now) const
{
    RepositoryStatusLookup result;
    result.validity = currentValidity;
    result.revision = currentRevision;
    if (currentValidity != Validity::Ready)
    {
        return result;
    }
    if (now < capturedAt || limits.freshness <= Clock::duration::zero() ||
        now - capturedAt >= limits.freshness)
    {
        result.validity = Validity::Stale;
        return result;
    }
    if ((!relativePath.isEmpty() && !validPath(relativePath)) || belowBoundary(relativePath))
    {
        return result;
    }
    const auto it = nodes.constFind(relativePath);
    const Node* node = it == nodes.cend() ? nullptr : &it.value();
    result.state = directState(relativePath, node);
    if (node != nullptr)
    {
        result.entry = node->entry;
        if (node->directory && options.recursive && !node->tracked)
        {
            result.state = std::max(result.state, node->aggregate);
        }
    }
    return result;
}

bool RepositoryStatusIndex::invalidate(Validity validity, quint64 revision)
{
    if (!validValidity(validity) || validity == Validity::Ready || revision == 0 ||
        revision <= currentRevision)
    {
        return false;
    }
    currentRevision = revision;
    discard(validity);
    return true;
}

void RepositoryStatusIndex::discard(Validity validity)
{
    nodes.clear();
    boundaries.clear();
    bytesUsed = 0;
    recordsUsed = 0;
    currentValidity = validity;
}

void RepositoryStatusIndex::clear() { discard(Validity::Unavailable); }

QString RepositoryStatusIndex::repositoryRoot() const { return root; }
quint64 RepositoryStatusIndex::revision() const { return currentRevision; }
qsizetype RepositoryStatusIndex::recordCount() const { return recordsUsed; }
qsizetype RepositoryStatusIndex::accountedBytes() const { return bytesUsed; }

} // namespace LinuxGitShell
