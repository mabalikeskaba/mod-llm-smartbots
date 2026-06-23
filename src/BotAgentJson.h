#ifndef MOD_BOT_AGENT_JSON_H
#define MOD_BOT_AGENT_JSON_H

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string>

// Minimal JSON helpers for the small, hand-built JSON payloads the module
// produces and the simple, well-formed bodies it receives from the C# service.
// The module has no JSON dependency; payloads are simple enough to handle by
// hand, and all dynamic strings pass through Escape().
namespace BotAgentJson
{
    inline std::string Escape(std::string const& in)
    {
        std::string out;
        out.reserve(in.size() + 8);
        for (char c : in)
        {
            switch (c)
            {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b";  break;
                case '\f': out += "\\f";  break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20)
                    {
                        // Control character -> \u00XX
                        static char const* hex = "0123456789abcdef";
                        out += "\\u00";
                        out += hex[(c >> 4) & 0xF];
                        out += hex[c & 0xF];
                    }
                    else
                    {
                        out += c;
                    }
                    break;
            }
        }
        return out;
    }

    // Convenience: a JSON error object {"error":"..."}.
    inline std::string Error(std::string const& reason)
    {
        return std::string(R"({"error":")") + Escape(reason) + "\"}";
    }

    // --- minimal field extraction (for well-formed bodies from the service) ---

    // Extracts the string value of "key" from a flat JSON object, decoding the
    // common escapes. Returns true on success. Not a general parser: it assumes
    // the simple, well-formed objects the C# service sends.
    inline bool GetString(std::string const& json, std::string const& key, std::string& out)
    {
        std::string needle = "\"" + key + "\"";
        size_t k = json.find(needle);
        if (k == std::string::npos)
            return false;

        size_t colon = json.find(':', k + needle.size());
        if (colon == std::string::npos)
            return false;

        size_t i = colon + 1;
        while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i])))
            ++i;
        if (i >= json.size() || json[i] != '"')
            return false;
        ++i; // opening quote

        std::string value;
        while (i < json.size() && json[i] != '"')
        {
            char c = json[i];
            if (c == '\\' && i + 1 < json.size())
            {
                char e = json[i + 1];
                switch (e)
                {
                    case 'n': value += '\n'; break;
                    case 't': value += '\t'; break;
                    case 'r': value += '\r'; break;
                    case '"': value += '"';  break;
                    case '\\': value += '\\'; break;
                    case '/': value += '/';  break;
                    default:  value += e;    break;
                }
                i += 2;
            }
            else
            {
                value += c;
                ++i;
            }
        }
        out = value;
        return true;
    }

    // Extracts an unsigned integer value of "key" (a JSON number). Returns the
    // parsed value, or `fallback` if the key is absent/non-numeric.
    inline uint32_t GetUInt(std::string const& json, std::string const& key, uint32_t fallback)
    {
        std::string needle = "\"" + key + "\"";
        size_t k = json.find(needle);
        if (k == std::string::npos)
            return fallback;

        size_t colon = json.find(':', k + needle.size());
        if (colon == std::string::npos)
            return fallback;

        size_t i = colon + 1;
        while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i])))
            ++i;
        if (i >= json.size() || !std::isdigit(static_cast<unsigned char>(json[i])))
            return fallback;

        return static_cast<uint32_t>(std::strtoul(json.c_str() + i, nullptr, 10));
    }
}

#endif // MOD_BOT_AGENT_JSON_H
