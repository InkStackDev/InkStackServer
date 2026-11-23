/**
 * @file I18n.hpp
 * @brief 国际化运行时：语言解析与 gettext 查表
 * @author Christins
 * @date 2026-10-18
 * @copyright GPL-3.0
 */
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <Chen/Http/Http.hpp>

class I18n {
public:
    static bool Init(const std::string& locale_dir);

    static std::string NormalizeLanguage(std::string_view language);

    static std::string ResolveLanguage(chen::http::HttpRequest::ptr request);

    static std::string Translate(std::string_view msgid, std::string_view language);

    static const std::vector<std::string>& SupportedLanguages();

    static std::string DefaultLanguage();
};
