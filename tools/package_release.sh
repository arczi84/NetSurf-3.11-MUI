#!/usr/bin/env bash
set -euo pipefail

STAMP="${1:-$(date +%Y-%m-%d-%H%M)}"
ARCHIVE_NAME="NetSurf-${STAMP}.lha"
PUBLISH_BRANCH="${PUBLISH_BRANCH:-release-packages}"

if [ ! -f "NetSurf" ]; then
    echo "NetSurf binary not found in $(pwd). Build it before packaging." >&2
    exit 1
fi

if ! command -v lha >/dev/null 2>&1; then
    echo "lha command missing. Install it (e.g. sudo apt install lhasa) and retry." >&2
    exit 1
fi

echo "Creating ${ARCHIVE_NAME}..."
lha -aq "${ARCHIVE_NAME}" NetSurf >/dev/null

echo "Archive ready: ${ARCHIVE_NAME}" 

ROOT_DIR="$(pwd)"
WORKTREE_DIR=".release-worktree-${STAMP}"

cleanup() {
    rm -f "${ROOT_DIR}/${ARCHIVE_NAME}"
    if [ -d "${WORKTREE_DIR}" ]; then
        git worktree remove --force "${WORKTREE_DIR}" >/dev/null 2>&1 || rm -rf "${WORKTREE_DIR}"
    fi
}

trap cleanup EXIT

rm -rf "${WORKTREE_DIR}"
git worktree add --detach "${WORKTREE_DIR}" >/dev/null

(
    cd "${WORKTREE_DIR}"

    remote_has_branch=false
    if git ls-remote --exit-code --heads origin "${PUBLISH_BRANCH}" >/dev/null 2>&1; then
        remote_has_branch=true
        git fetch origin "${PUBLISH_BRANCH}" >/dev/null 2>&1 || true
    fi

    if ${remote_has_branch}; then
        git checkout -B "${PUBLISH_BRANCH}" "origin/${PUBLISH_BRANCH}" >/dev/null 2>&1
    elif git show-ref --verify --quiet "refs/heads/${PUBLISH_BRANCH}"; then
        git checkout "${PUBLISH_BRANCH}" >/dev/null 2>&1
    else
        git checkout --orphan "${PUBLISH_BRANCH}" >/dev/null 2>&1
        git rm -rf . >/dev/null 2>&1 || true
    fi

    cp "${ROOT_DIR}/${ARCHIVE_NAME}" .
    git add "${ARCHIVE_NAME}"
    git commit -m "Release archive ${ARCHIVE_NAME}" >/dev/null
    git push origin "${PUBLISH_BRANCH}" >/dev/null
)

trap - EXIT
cleanup

echo "Release committed to branch ${PUBLISH_BRANCH} and local archive removed."