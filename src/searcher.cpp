#include "searcher.h"

#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>

// how many threads to spin up - using hardware_concurrency as baseline
// on most machines this is # of logical cores. I/O bound tasks usually
// benefit from going a bit higher than core count but lets keep it simple
static size_t chooseThreadCount() {
    unsigned int hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 4; // fallback if the OS doesnt tell us
    return static_cast<size_t>(hw);
}

Searcher::Searcher(SearchConfig cfg) : config(std::move(cfg)) {
    pool = std::make_unique<ThreadPool>(chooseThreadCount());

    if (config.useRegex) {
        auto flags = std::regex_constants::ECMAScript | std::regex_constants::optimize;
        if (config.ignoreCase)
            flags |= std::regex_constants::icase;
        pattern = std::regex(config.pattern, flags);
    }
}

std::vector<FileResult> Searcher::run() {
    fs::path root(config.rootPath);

    if (!fs::exists(root)) {
        std::cerr << "error: path does not exist: " << root << "\n";
        return {};
    }

    if (fs::is_regular_file(root)) {
        // user passed a single file directly, just search that
        searchFile(root);
    } else {
        walkDirectory(root, 0);
    }

    pool->waitAll();

    // sort by filepath so output is deterministic
    // (threads finish in random order obviously)
    std::sort(results.begin(), results.end(), [](const FileResult& a, const FileResult& b){
        return a.filepath < b.filepath;
    });

    return results;
}

void Searcher::walkDirectory(const fs::path& dir, int depth) {
    if (config.maxDepth >= 0 && depth > config.maxDepth)
        return;

    std::error_code ec;
    for (auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) {
            // permission denied etc - just skip silently
            // could add a verbose flag to print these later
            ec.clear();
            continue;
        }

        if (entry.is_symlink()) continue; // skip symlinks, avoids cycles

        if (entry.is_directory()) {
            walkDirectory(entry.path(), depth + 1);
        } else if (entry.is_regular_file()) {
            if (!extensionAllowed(entry.path())) continue;

            // capture path by value - important, iterator will advance
            fs::path p = entry.path();
            pool->enqueue([this, p]() {
                searchFile(p);
            });
        }
    }
}

void Searcher::searchFile(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return; // can happen if file was deleted between walk and search

    numFilesScanned++;

    FileResult result;
    result.filepath = path.string();

    std::string line;
    int lineNum = 0;

    while (std::getline(file, line)) {
        lineNum++;

        // skip binary files heuristically - if we see a null byte in first
        // 512 chars we probably dont want to print garbage to terminal
        // this is the same trick git uses basically
        if (lineNum == 1) {
            size_t checkLen = std::min(line.size(), (size_t)512);
            for (size_t i = 0; i < checkLen; i++) {
                if (line[i] == '\0') goto nextfile; // yeah i know, goto, sue me
            }
        }

        {
            auto matches = findMatches(line, lineNum);
            for (auto& m : matches) {
                result.matches.push_back(std::move(m));
                numTotalMatches++;
            }
        }
    }

nextfile:
    if (result.hasMatches()) {
        numFilesMatched++;
        std::lock_guard<std::mutex> lock(resultsMutex);
        results.push_back(std::move(result));
    }
}

// returns all matches on this line (there can be multiple)
std::vector<Match> Searcher::findMatches(const std::string& line, int lineNum) {
    std::vector<Match> found;

    if (config.useRegex) {
        // regex search path
        auto begin = std::sregex_iterator(line.begin(), line.end(), pattern);
        auto end   = std::sregex_iterator();

        for (auto it = begin; it != end; ++it) {
            Match m;
            m.lineNum     = lineNum;
            m.lineContent = line;
            m.matchStart  = it->position();
            m.matchLen    = it->length();
            found.push_back(m);
        }
    } else {
        // plain string search - faster than regex for simple patterns
        std::string haystack = line;
        std::string needle   = config.pattern;

        if (config.ignoreCase) {
            // lowercase both for comparison
            std::transform(haystack.begin(), haystack.end(), haystack.begin(), ::tolower);
            std::transform(needle.begin(),   needle.end(),   needle.begin(),   ::tolower);
        }

        size_t pos = 0;
        while ((pos = haystack.find(needle, pos)) != std::string::npos) {
            Match m;
            m.lineNum     = lineNum;
            m.lineContent = line; // original line, not lowercased
            m.matchStart  = pos;
            m.matchLen    = needle.size();
            found.push_back(m);
            pos += needle.size(); // advance past this match
        }
    }

    return found;
}

bool Searcher::extensionAllowed(const fs::path& p) const {
    if (config.includeExts.empty()) return true; // no filter = allow everything

    std::string ext = p.extension().string();
    // case insensitive ext check
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    for (const auto& allowed : config.includeExts) {
        std::string a = allowed;
        std::transform(a.begin(), a.end(), a.begin(), ::tolower);
        if (ext == a) return true;
    }
    return false;
}
