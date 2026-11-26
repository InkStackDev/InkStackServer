/**
 * @file I18n.hpp
 * @brief 语言解析：从请求参数 / Accept-Language / 系统设置中解析 zh_CN 或 en
 * @author Christins
 * @date 2026-10-18
 * @copyright GPL-3.0
 */
#pragma once

#include <string>
#include <string_view>

#include <Chen/Http/Http.hpp>

class I18n {
public:
    static std::string NormalizeLanguage(std::string_view language);

    static std::string ResolveLanguage(chen::http::HttpRequest::ptr request);

    static std::string DefaultLanguage();
};
