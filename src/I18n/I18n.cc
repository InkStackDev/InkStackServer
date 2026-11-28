#include "I18n.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <utility>
#include <vector>

#include <Chen/Util/StringUtil.hpp>

#include "../Cache/SystemSettingsStore.hpp"

constexpr const char* kDefaultLanguage = "zh-CN";
const std::string kTrimChars = " \t\r\n\v\f";

double ParseQuality(const std::string& value)
{
    const std::string text = chen::StringUtil::Trim(value, kTrimChars);
    if (text.size() < 3) {
        return -1.0;
    }
    if ((text[0] != 'q' && text[0] != 'Q') || text[1] != '=') {
        return -1.0;
    }
    const std::string number = chen::StringUtil::Trim(text.substr(2), kTrimChars);
    if (number.empty()) {
        return -1.0;
    }
    char* end = nullptr;
    const double quality = std::strtod(number.c_str(), &end);
    if (end == number.c_str() || *end != '\0' || quality < 0.0 || quality > 1.0) {
        return -1.0;
    }
    return quality;
}

std::string BestFromAcceptLanguage(const std::string& header)
{
    if (header.empty()) {
        return "";
    }

    struct Candidate {
        double quality;
        std::string language;
    };

    std::vector<Candidate> candidates;
    size_t start = 0;
    while (start <= header.size()) {
        const size_t comma = header.find(',', start);

        std::string str = header.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        const std::string item = chen::StringUtil::Trim(str, kTrimChars);

        if (!item.empty()) {
            std::string tag;
            double quality = 1.0;
            bool first = true;
            bool valid = true;
            size_t pos = 0;
            while (pos <= item.size()) {
                const size_t semicolon = item.find(';', pos);

                std::string pt = item.substr(pos, semicolon == std::string::npos ? std::string::npos : semicolon - pos);
                const std::string part = chen::StringUtil::Trim(pt, kTrimChars);

                if (first) {
                    tag = part;
                    first = false;
                } else if (part.size() > 1 && (part[0] == 'q' || part[0] == 'Q') && part[1] == '=') {
                    const double parsed = ParseQuality(part);
                    if (parsed < 0.0) {
                        valid = false;
                        break;
                    }
                    quality = parsed;
                }
                if (semicolon == std::string::npos) {
                    break;
                }
                pos = semicolon + 1;
            }

            if (valid && quality > 0.0 && !tag.empty() && tag != "*") {
                std::string language = I18n::NormalizeLanguage(tag);
                if (!language.empty()) {
                    candidates.push_back({quality, std::move(language)});
                }
            }
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }

    std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
        return lhs.quality > rhs.quality;
    });

    return candidates.empty() ? "" : candidates.front().language;
}

std::string I18n::DefaultLanguage()
{
    return kDefaultLanguage;
}

std::string I18n::NormalizeLanguage(std::string_view language)
{
    std::string value = chen::StringUtil::Trim(std::string(language), kTrimChars);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    std::replace(value.begin(), value.end(), '_', '-');

    if (value == "zh" || value.rfind("zh-", 0) == 0) {
        return "zh-CN";
    }
    if (value == "en" || value.rfind("en-", 0) == 0) {
        return "en-US";
    }
    return "";
}

std::string I18n::ResolveLanguage(chen::http::HttpRequest::ptr request)
{
    if (request) {
        const std::string query = request->getParam("lang");
        std::string normalized = NormalizeLanguage(query);
        if (!normalized.empty()) {
            return normalized;
        }

        normalized = BestFromAcceptLanguage(request->getHeader("Accept-Language"));
        if (!normalized.empty()) {
            return normalized;
        }
    }

    auto settings = SystemSettingsStoreRegistry::GetInstance()->get();
    if (settings) {
        std::string normalized = NormalizeLanguage(settings->getLanguage());
        if (!normalized.empty()) {
            return normalized;
        }
    }

    return DefaultLanguage();
}
