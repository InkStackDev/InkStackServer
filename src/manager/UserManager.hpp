#pragma once

#include "DB/UserInfo.hpp"

#include <Chen/Util/Singleton.hpp>

class UserManager {
public:
    enum Role {
        USER = 1,
        ADMIN = 2,
        EDITOR = 3,
        CHECKER = 4
    };
    enum Status {
        INACTIVE = 0,
        ACTIVE = 1,
        BANNED = 2
    };

    void add(DB::UserInfo::ptr info);

    void update(DB::UserInfo::ptr info);

    void getAllIds(std::vector<int64_t>& ids, bool is_valid);

    /**
     * @brief 确保存在超级管理员（启动时无管理员则按配置自动创建）
     */
    void ensureSuperAdmin();

    uint64_t listByPages(std::vector<DB::UserInfo::ptr>& infos,
        uint64_t offset,
        uint64_t size,
        int32_t role,
        int32_t state,
        int32_t days,
        bool is_valid);

    DB::UserInfo::ptr get(int64_t id);

    DB::UserInfo::ptr getByAccount(const std::string& v);

    DB::UserInfo::ptr getByEmail(const std::string& v);

    DB::UserInfo::ptr getByName(const std::string& v);

    static std::string GetToken(DB::UserInfo::ptr info, int64_t us);

    static std::string GenerateToken();
};

typedef chen::Singleton<UserManager> UserMgr;
