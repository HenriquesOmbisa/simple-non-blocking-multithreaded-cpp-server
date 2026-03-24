#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace mojoraw::core {

// ── Context types ─────────────────────────────────────────────────────────────

/// Flat key→value map for a single object row (used inside list context values)
using RowContext = std::unordered_map<std::string, std::string>;

/// A context value is either a plain string or a list of RowContext objects.
/// Implicit conversions from string / const char* / vector keep the call-site API unchanged.
struct ContextValue {
    std::string             str;
    std::vector<RowContext> rows;
    bool                    isList{false};

    ContextValue() = default;
    /* implicit */ ContextValue(std::string s)           : str(std::move(s))  {}
    /* implicit */ ContextValue(const char* s)           : str(s)             {}
    /* implicit */ ContextValue(std::vector<RowContext> r): rows(std::move(r)), isList(true) {}
};

/// Top-level context map passed to renderFile / renderString
using Context = std::unordered_map<std::string, ContextValue>;

// ─────────────────────────────────────────────────────────────────────────────

class TemplateEngine {
public:
    // Re-export so callers can use TemplateEngine::Context etc.
    using RowContext   = mojoraw::core::RowContext;
    using ContextValue = mojoraw::core::ContextValue;
    using Context      = mojoraw::core::Context;

    static std::string renderString(const std::string& source, const Context& ctx) {
        return renderStringInternal(source, ctx, std::filesystem::current_path(), 0);
    }

    static bool renderFile(const std::string& filePath,
                           const Context& ctx,
                           std::string& output) {
        const std::filesystem::path path(filePath);
        std::string source;
        if (!readFile(path, source)) return false;

        const std::filesystem::path base = path.has_parent_path()
            ? path.parent_path()
            : std::filesystem::current_path();

        output = renderStringInternal(source, ctx, base, 0);
        return true;
    }

private:
    static constexpr int MAX_TEMPLATE_DEPTH = 16;

    // ── File-content cache (thread-safe) ──────────────────────────────────────
    struct CacheEntry {
        std::filesystem::file_time_type mtime;
        std::string                     content;
    };

    static std::mutex& cacheMutex() {
        static std::mutex m;
        return m;
    }
    static std::unordered_map<std::string, CacheEntry>& fileCache() {
        static std::unordered_map<std::string, CacheEntry> c;
        return c;
    }

    static bool readFile(const std::filesystem::path& path, std::string& out) {
        const std::string key = path.string();
        std::error_code ec;
        const auto mtime = std::filesystem::last_write_time(path, ec);
        if (ec) return false;

        {
            std::lock_guard<std::mutex> lk(cacheMutex());
            auto it = fileCache().find(key);
            if (it != fileCache().end() && it->second.mtime == mtime) {
                out = it->second.content;
                return true;
            }
        }

        std::ifstream in(path, std::ios::binary);
        if (!in) return false;
        std::ostringstream ss;
        ss << in.rdbuf();
        std::string content = ss.str();

        {
            std::lock_guard<std::mutex> lk(cacheMutex());
            fileCache()[key] = {mtime, content};
        }
        out = std::move(content);
        return true;
    }

    // ── Helpers ───────────────────────────────────────────────────────────────
    static std::string trim(std::string s) {
        auto notSpace = [](unsigned char c) { return !std::isspace(c); };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
        s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
        return s;
    }

    static std::vector<std::string> split(const std::string& input, char delim) {
        std::vector<std::string> out;
        std::stringstream ss(input);
        std::string token;
        while (std::getline(ss, token, delim)) {
            token = trim(token);
            if (!token.empty()) out.push_back(token);
        }
        return out;
    }

    static bool isTruthy(const std::string& value) {
        std::string low = value;
        std::transform(low.begin(), low.end(), low.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return !(low.empty() || low == "0" || low == "false"
              || low == "no"  || low == "off" || low == "null");
    }

    static std::string escapeHtml(const std::string& in) {
        std::string out;
        out.reserve(in.size());
        for (char c : in) {
            switch (c) {
                case '&':  out += "&amp;";  break;
                case '<':  out += "&lt;";   break;
                case '>':  out += "&gt;";   break;
                case '"':  out += "&quot;"; break;
                case '\'': out += "&#39;";  break;
                default:   out.push_back(c); break;
            }
        }
        return out;
    }

    // Resolve quoted literal or context key to its string value
    static std::string evalValueToken(const std::string& token, const Context& ctx) {
        std::string t = trim(token);
        if (t.empty()) return "";
        if ((t.front() == '"' && t.back() == '"')
         || (t.front() == '\'' && t.back() == '\''))
            return t.substr(1, t.size() - 2);
        auto it = ctx.find(t);
        return (it != ctx.end() && !it->second.isList) ? it->second.str : "";
    }

    static bool evalCondition(const std::string& expr, const Context& ctx) {
        const std::string e = trim(expr);
        if (e.empty()) return false;

        // key == value  |  key != value
        static const std::regex eqRe(
            R"(^([A-Za-z_][A-Za-z0-9_.]*)\s*(==|!=)\s*(.+)$)");
        std::smatch m;
        if (std::regex_match(e, m, eqRe)) {
            const std::string key = m[1].str();
            const std::string op  = m[2].str();
            const std::string rhs = evalValueToken(m[3].str(), ctx);
            std::string lhs;
            auto it = ctx.find(key);
            if (it != ctx.end() && !it->second.isList) lhs = it->second.str;
            return (op == "==") ? (lhs == rhs) : (lhs != rhs);
        }

        // !flag
        static const std::regex notRe(R"(^!\s*([A-Za-z_][A-Za-z0-9_.]*)$)");
        if (std::regex_match(e, m, notRe)) {
            auto it = ctx.find(m[1].str());
            return !(it != ctx.end() && !it->second.isList && isTruthy(it->second.str));
        }

        // truthy check
        auto it = ctx.find(e);
        return it != ctx.end() && !it->second.isList && isTruthy(it->second.str);
    }

    static std::string applyFilters(const std::string& value,
                                    const std::vector<std::string>& filters,
                                    bool rawOutput) {
        std::string out = value;
        for (const auto& f : filters) {
            if (f == "upper") {
                std::transform(out.begin(), out.end(), out.begin(),
                    [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            } else if (f == "lower") {
                std::transform(out.begin(), out.end(), out.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            } else if (f == "trim") {
                out = trim(out);
            } else if (f == "escape") {
                out = escapeHtml(out);
            }
        }
        if (!rawOutput) out = escapeHtml(out);
        return out;
    }

    // ── Main render pipeline ──────────────────────────────────────────────────
    static std::string renderStringInternal(const std::string& source,
                                            const Context& ctx,
                                            const std::filesystem::path& baseDir,
                                            int depth) {
        if (depth > MAX_TEMPLATE_DEPTH)
            return "<!-- template recursion limit reached -->";

        std::string out = source;

        // Order matters: extends → comments → for → if → includes → vars
        out = resolveExtends(out, ctx, baseDir, depth);
        applyComments(out);
        applyForBlocks(out, ctx, baseDir, depth);
        applyIfBlocks(out, ctx, baseDir, depth);
        applyIncludes(out, ctx, baseDir, depth);
        applyVariables(out, ctx, true);   // {{{ raw }}}
        applyVariables(out, ctx, false);  // {{ escaped }}

        return out;
    }

    // ── Layout inheritance ────────────────────────────────────────────────────
    // Child:  {% extends "layout.mj.html" %}
    //         {% block title %}My Page{% endblock %}
    // Layout: <title>{% block title %}Default{% endblock %}</title>
    static std::string resolveExtends(const std::string& source,
                                      const Context& ctx,
                                      const std::filesystem::path& baseDir,
                                      int depth) {
        static const std::regex extendsRe(
            R"(\{%\s*extends\s+["']([^"']+)["']\s*%\})");
        static const std::regex blockRe(
            R"(\{%\s*block\s+(\w+)\s*%\}([\s\S]*?)\{%\s*endblock\s*%\})");

        std::smatch em;
        if (!std::regex_search(source, em, extendsRe)) return source;  // not a child

        // — collect overriding blocks from child —
        std::unordered_map<std::string, std::string> childBlocks;
        {
            std::sregex_iterator bit(source.begin(), source.end(), blockRe), bend;
            for (; bit != bend; ++bit)
                childBlocks[(*bit)[1].str()] = (*bit)[2].str();
        }

        // — load layout —
        const std::filesystem::path layoutPath = baseDir / em[1].str();
        std::string layoutSource;
        if (!readFile(layoutPath, layoutSource))
            return "<!-- layout not found: " + em[1].str() + " -->";

        // — merge: replace layout blocks with child overrides (or keep default) —
        std::string merged;
        std::size_t cursor = 0;
        std::sregex_iterator lit(layoutSource.begin(), layoutSource.end(), blockRe), lend;
        for (; lit != lend; ++lit) {
            const auto& bm = *lit;
            merged += layoutSource.substr(cursor,
                static_cast<std::size_t>(bm.position(0)) - cursor);
            auto cit = childBlocks.find(bm[1].str());
            merged += (cit != childBlocks.end()) ? cit->second : bm[2].str();
            cursor = static_cast<std::size_t>(bm.position(0))
                   + static_cast<std::size_t>(bm.length(0));
        }
        merged += layoutSource.substr(cursor);

        return renderStringInternal(merged, ctx, layoutPath.parent_path(), depth + 1);
    }

    // ── Comment stripping ─────────────────────────────────────────────────────
    static void applyComments(std::string& text) {
        static const std::regex commentRe(R"(\{#[\s\S]*?#\})");
        text = std::regex_replace(text, commentRe, "");
    }

    // ── Includes ──────────────────────────────────────────────────────────────
    static void applyIncludes(std::string& text,
                              const Context& ctx,
                              const std::filesystem::path& baseDir,
                              int depth) {
        static const std::regex includeRe(
            R"(\{%\s*include\s+["']([^"']+)["']\s*%\})");
        std::smatch m;
        while (std::regex_search(text, m, includeRe)) {
            const std::filesystem::path incPath = baseDir / m[1].str();
            std::string replacement;
            std::string incSource;
            if (readFile(incPath, incSource))
                replacement = renderStringInternal(incSource, ctx,
                                                   incPath.parent_path(), depth + 1);
            text.replace(m.position(0), static_cast<std::size_t>(m.length(0)), replacement);
        }
    }

    // ── If / elseif / else / endif ────────────────────────────────────────────
    static void applyIfBlocks(std::string& text,
                              const Context& ctx,
                              const std::filesystem::path& baseDir,
                              int depth) {
        static const std::regex ifBlockRe(
            R"(\{%\s*if\s+(.+?)\s*%\}([\s\S]*?)\{%\s*endif\s*%\})");
        static const std::regex splitRe(
            R"(\{%\s*(elseif\s+.+?|else)\s*%\})");

        std::smatch m;
        while (std::regex_search(text, m, ifBlockRe)) {
            const std::string firstCond = trim(m[1].str());
            const std::string body      = m[2].str();

            std::vector<std::pair<std::string, std::string>> branches;
            std::string elseBranch;
            std::size_t cursor      = 0;
            std::string currentCond = firstCond;

            std::sregex_iterator it(body.begin(), body.end(), splitRe), end;
            for (; it != end; ++it) {
                const std::size_t pos = static_cast<std::size_t>((*it).position());
                branches.push_back({currentCond, body.substr(cursor, pos - cursor)});
                const std::string tag = trim((*it)[1].str());
                if (tag.rfind("elseif", 0) == 0)
                    currentCond = trim(tag.substr(6));
                else
                    currentCond.clear();
                cursor = pos + static_cast<std::size_t>((*it).length());
            }

            const std::string tail = body.substr(cursor);
            if (currentCond.empty()) elseBranch = tail;
            else branches.push_back({currentCond, tail});

            std::string replacement;
            bool matched = false;
            for (const auto& b : branches) {
                if (evalCondition(b.first, ctx)) {
                    replacement = renderStringInternal(b.second, ctx, baseDir, depth + 1);
                    matched = true;
                    break;
                }
            }
            if (!matched && !elseBranch.empty())
                replacement = renderStringInternal(elseBranch, ctx, baseDir, depth + 1);

            text.replace(m.position(0), static_cast<std::size_t>(m.length(0)), replacement);
        }
    }

    // ── For loop ──────────────────────────────────────────────────────────────
    // Rich list  → vector<RowContext>: {{ item.field }} syntax
    // CSV string → comma-separated:   {{ item }} syntax (backward compat)
    static void applyForBlocks(std::string& text,
                               const Context& ctx,
                               const std::filesystem::path& baseDir,
                               int depth) {
        static const std::regex forRe(
            R"(\{%\s*for\s+([A-Za-z_][A-Za-z0-9_]*)\s+in\s+([A-Za-z_][A-Za-z0-9_.]*)\s*%\}([\s\S]*?)\{%\s*endfor\s*%\})");

        std::smatch m;
        while (std::regex_search(text, m, forRe)) {
            const std::string itemVar = m[1].str();
            const std::string listKey = m[2].str();
            const std::string block   = m[3].str();

            std::string replacement;
            auto it = ctx.find(listKey);
            if (it != ctx.end()) {
                if (it->second.isList) {
                    // Rich list: merge "item.field" keys into child context
                    for (const auto& row : it->second.rows) {
                        Context child = ctx;
                        for (const auto& [k, v] : row)
                            child[itemVar + "." + k] = ContextValue(v);
                        if (!row.empty())
                            child[itemVar] = ContextValue(row.begin()->second);
                        replacement += renderStringInternal(block, child, baseDir, depth + 1);
                    }
                } else {
                    // Legacy CSV fallback
                    for (const auto& item : split(it->second.str, ',')) {
                        Context child = ctx;
                        child[itemVar] = ContextValue(item);
                        replacement += renderStringInternal(block, child, baseDir, depth + 1);
                    }
                }
            }
            text.replace(m.position(0), static_cast<std::size_t>(m.length(0)), replacement);
        }
    }

    // ── Variable output ───────────────────────────────────────────────────────
    static void applyVariables(std::string& text, const Context& ctx, bool rawOutput) {
        const std::regex varRe = rawOutput
            ? std::regex(R"(\{\{\{\s*([^{}]+?)\s*\}\}\})")
            : std::regex(R"(\{\{\s*([^{}]+?)\s*\}\})");

        std::smatch m;
        while (std::regex_search(text, m, varRe)) {
            const std::string expr = trim(m[1].str());
            std::vector<std::string> parts = split(expr, '|');

            std::string key;
            std::vector<std::string> filters;
            if (!parts.empty()) {
                key = trim(parts.front());
                filters.assign(parts.begin() + 1, parts.end());
                for (auto& f : filters) f = trim(f);
            }

            std::string value;
            auto it = ctx.find(key);
            if (it != ctx.end() && !it->second.isList) value = it->second.str;

            const std::string replacement = applyFilters(value, filters, rawOutput);
            text.replace(m.position(0), static_cast<std::size_t>(m.length(0)), replacement);
        }
    }
};

} // namespace mojoraw::core
