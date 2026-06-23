#ifndef MOD_BOT_AGENT_JSON_H
#define MOD_BOT_AGENT_JSON_H

#include <string>

// Minimal JSON string escaping for the small, hand-built JSON payloads the
// module produces. The module has no JSON dependency; payloads are simple
// enough to assemble by hand, and all dynamic strings pass through here.
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
}

#endif // MOD_BOT_AGENT_JSON_H
