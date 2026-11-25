/**
 * @file ErrorCodes.hpp
 * @brief 统一业务错误码常量定义
 * @author Christins
 * @date 2026-06-11
 * @copyright Apache 2.0
 *
 * 错误码格式: XXYYY (5位数字)
 *   XX  = 模块号 (00=成功, 01=通用, 02=用户, 03=文章, ...)
 *   YYY = 模块内错误序号 (001-999)
 *
 * 与 errors.json 保持同步
 */
#pragma once

static constexpr int SUCCESS = 0; // 成功

enum ParamError {
    PARAM_MISSING = 1001,  // 缺少必填参数
    PARAM_INVALID = 1002,  // 参数格式错误
    INVALID_METHOD = 1003, // 不支持的请求方法
};

enum AuthError {
    NOT_LOGIN = 1101,         // 未登录
    ACCESS_DENIED = 1102,     // 权限不足
    ACCOUNT_INVALID = 1103,   // 账号状态异常
    AUTH_CODE_INVALID = 1104, // 验证码错误
};

enum UserError {
    USER_NOT_FOUND = 2001,               // 用户不存在
    USER_ACCOUNT_EXISTS = 2002,          // 账号已存在
    USER_EMAIL_EXISTS = 2003,            // 邮箱已注册
    USER_INVALID_ACCOUNT = 2004,         // 账号格式错误
    USER_INVALID_EMAIL = 2005,           // 邮箱格式错误
    USER_INVALID_PASSWORD = 2006,        // 密码格式错误
    USER_PASSWORD_WRONG = 2007,          // 密码错误
    USER_OLD_PASSWORD_WRONG = 2008,      // 旧密码错误
    USER_ALREADY_DELETED = 2009,         // 用户已删除
    USER_ALREADY_LOGIN = 2010,           // 已登录
    USER_EMAIL_NOT_REGISTER = 2011,      // 邮箱未注册
    USER_NO_PARAM = 2012,                // 缺少更新参数
    USER_PASSWORDS_DIFFER = 2013,        // 两次密码不一致
    USER_CANNOT_FOLLOW_SELF = 2014,      // 不能关注自己
    USER_NOT_FOLLOWING = 2015,           // 未关注该用户
    USER_FOLLOW_FAILED = 2016,           // 关注操作失败
    SEND_CODE_FREQUENT = 2017,           // 验证码发送过于频繁
    DEVICE_NOT_FOUND = 2018,             // 登录设备不存在
    DEVICE_KICK_FAILED = 2019,           // 设备下线失败
    TOTP_INVALID_CODE = 2020,            // TOTP验证码错误
    TOTP_NOT_ENABLED = 2021,             // 2FA未启用
    TOTP_ALREADY_ENABLED = 2022,         // 2FA已启用
    TOTP_ENABLE_FAILED = 2023,           // 2FA启用失败
    TOTP_CONFIRM_FAILED = 2024,          // 2FA确认失败
    RECOVERY_CODE_INVALID = 2025,        // 恢复码无效或已使用
    TEMP_TOKEN_INVALID = 2026,           // 临时凭证无效或已过期
    GITHUB_OAUTH_FAILED = 2027,          // GitHub 授权登录失败
    GITHUB_STATE_INVALID = 2028,         // OAuth state 无效或已过期
    PASSKEY_NOT_FOUND = 2029,            // Passkey 不存在
    PASSKEY_ALREADY_EXISTS = 2030,       // Passkey 已注册
    PASSKEY_REGISTER_FAILED = 2031,      // Passkey 注册失败
    WEBAUTHN_NOT_CONFIGURED = 2032,      // WebAuthn 未配置
    WEBAUTHN_CHALLENGE_INVALID = 2033,   // WebAuthn challenge 无效或已过期
    WEBAUTHN_ORIGIN_MISMATCH = 2034,     // WebAuthn origin 不匹配
    WEBAUTHN_RP_ID_MISMATCH = 2035,      // WebAuthn rpId 不匹配
    WEBAUTHN_INVALID_CREDENTIAL = 2036,  // WebAuthn 凭据无效
    WEBAUTHN_UNSUPPORTED_ALG = 2037,     // WebAuthn 不支持的算法
    WEBAUTHN_VERIFY_FAILED = 2038,       // WebAuthn 校验失败
    EMAIL_CODE_ATTEMPTS_EXCEEDED = 2039, // 验证码尝试次数过多
};

enum ArticleError {
    ARTICLE_NOT_FOUND = 3001,          // 文章不存在
    ARTICLE_INVALID_TYPE = 3002,       // 文章类型错误
    ARTICLE_INVALID_STATE = 3003,      // 文章状态错误
    ARTICLE_INVALID_ID = 3004,         // 文章ID无效
    ARTICLE_INSERT_FAILED = 3005,      // 创建文章失败
    ARTICLE_UPDATE_FAILED = 3006,      // 更新文章失败
    ARTICLE_DELETE_FAILED = 3007,      // 删除文章失败
    ARTICLE_PUBLISH_FAILED = 3008,     // 发布文章失败
    ARTICLE_CATEGORY_EMPTY = 3009,     // 分类参数为空
    ARTICLE_SEARCH_UNAVAILABLE = 3010, // 搜索服务不可用
};

enum FileError {
    FILE_NOT_FOUND = 4001,              // 文件不存在
    FILE_TOO_LARGE = 4002,              // 文件过大
    FILE_INVALID_NAME = 4003,           // 文件名无效
    FILE_INVALID_PATH = 4004,           // 文件路径无效
    FILE_READ_ERROR = 4005,             // 文件读取失败
    FILE_UPLOAD_FAILED = 4006,          // 文件上传失败
    FILE_SAVE_FAILED = 4007,            // 文件保存失败
    FILE_PROTOCOL_ERROR = 4008,         // 上传协议错误
    FILE_DUMP_FAILED = 4009,            // 文件转储失败
    UPLOAD_SESSION_NOT_FOUND = 4010,    // 上传会话不存在
    UPLOAD_INVALID_TYPE = 4011,         // 上传类型无效
    UPLOAD_INVALID_PARAMS = 4012,       // 上传参数无效
    UPLOAD_CHUNK_INVALID = 4013,        // 分片参数无效
    UPLOAD_CHUNK_ALREADY_DONE = 4014,   // 分片已接收
    UPLOAD_CHUNK_INCOMPLETE = 4015,     // 分片未全部上传
    UPLOAD_FILE_CREATE_FAILED = 4016,   // 创建文件失败
    UPLOAD_FILE_ASSEMBLE_FAILED = 4017, // 文件合并失败
    UPLOAD_FILE_HASH_FAILED = 4018,     // 文件哈希计算失败
    UPLOAD_TEMP_DIR_FAILED = 4019,      // 创建临时目录失败
    UPLOAD_TEMP_FILE_FAILED = 4020,     // 创建临时文件失败
    UPLOAD_CHUNK_SIZE_EXCEED = 4021,    // 分片大小超出声明值
    UPLOAD_TYPE_INVALID_TYPE = 4022,    // 上传文件类型无效
    UPLOAD_FILE_EMPTY = 4023,           // 上传文件为空
};

enum OrganizationError {
    ORG_NOT_FOUND = 5001,              // 组织不存在
    ORG_DISABLED = 5002,               // 组织已禁用
    ORG_ALREADY_JOINED = 5003,         // 已加入该组织
    ORG_ALREADY_APPLIED = 5004,        // 已申请加入该组织
    ORG_APPLY_NOT_FOUND = 5005,        // 申请不存在
    ORG_APPLY_ALREADY_REVIEWED = 5006, // 申请已审核
    ORG_INVALID_ROLE = 5007,           // 无效的组织角色
    ORG_INVALID_STATE = 5008,          // 无效的组织状态
    ORG_NOT_MEMBER = 5009,             // 不是组织成员
    ORG_INVALID_ID = 5010,             // 组织ID无效
    ORG_INSERT_FAILED = 5011,          // 创建组织失败
    ORG_UPDATE_FAILED = 5012,          // 更新组织失败
    ORG_DELETE_FAILED = 5013,          // 删除组织失败
    ORG_NO_VALID_ORGS = 5014,          // 没有有效的组织
    ORG_APPLY_INSERT_FAILED = 5015,    // 提交申请失败
    ORG_APPLY_UPDATE_FAILED = 5016,    // 更新申请失败
    ORG_USER_REL_FAILED = 5017,        // 组织成员操作失败
    ORG_REPO_NOT_FOUND = 5018,         // 仓库不存在
    ORG_REPO_NAME_EXISTS = 5019,       // 同组织下仓库名已存在
    ORG_PR_NOT_FOUND = 5020,           // PR 记录不存在
    ORG_REPO_TOKEN_MISSING = 5021,     // 仓库未配置 GitHub Token
    GITHUB_PR_NOT_FOUND = 5022,        // GitHub 上未找到该 PR
    GITHUB_RATE_LIMITED = 5023,        // GitHub API 触发限流
    GITHUB_API_FAILED = 5024,          // GitHub API 请求失败
    ORG_PR_SYNC_IN_PROGRESS = 5025,    // 该 PR 正在同步中
    ORG_OWNER_CANNOT_LEAVE = 5026,     // 组织所有者不能直接退出组织
    ORG_OWNER_CANNOT_BE_KICKED = 5027, // 不能移除组织所有者
    ORG_OWNER_ROLE_PROTECTED = 5028,   // 不能修改组织所有者的角色
};

enum AssignmentError {
    ASSIGNMENT_NOT_FOUND = 6001,       // 作业不存在
    ASSIGNMENT_INVALID_ID = 6002,      // 作业ID无效
    ASSIGNMENT_SIZE_EXCEED = 6003,     // 作业文件大小超限
    ASSIGNMENT_MAX_SIZE_EXCEED = 6004, // 提交数量超限
    ASSIGNMENT_INSERT_FAILED = 6005,   // 创建作业失败
    ASSIGNMENT_UPDATE_FAILED = 6006,   // 更新作业失败
    ASSIGNMENT_DELETE_FAILED = 6007,   // 删除作业失败
};

enum CommentError {
    COMMENT_NOT_FOUND = 7001,         // 评论不存在
    COMMENT_PERMISSION_DENIED = 7002, // 无权操作此评论
    COMMENT_CREATE_FAILED = 7003,     // 创建评论失败
    COMMENT_UPDATE_FAILED = 7004,     // 更新评论失败
    COMMENT_DELETE_FAILED = 7005,     // 删除评论失败
    COMMENT_PRAISE_FAILED = 7006,     // 点赞评论失败
    COMMENT_UNPRAISE_FAILED = 7007,   // 取消点赞评论失败
    COMMENT_PARENT_NOT_FOUND = 7008,  // 父评论不存在
};

enum RDError {
    BUG_NOT_FOUND = 8001,         // Bug不存在
    REQUIREMENT_NOT_FOUND = 8002, // 需求不存在
    TASK_NOT_FOUND = 8003,        // 任务不存在
    RD_INVALID_TYPE = 8004,       // 无效的工单类型
    RD_NOT_ORG_MEMBER = 8005,     // 不是该组织成员
    RD_INSERT_FAILED = 8006,      // 创建工单失败
    RD_UPDATE_FAILED = 8007,      // 更新工单失败
    RD_DELETE_FAILED = 8008,      // 删除工单失败
    RD_INVALID_USER = 8009,       // 用户无效
};

enum SystemError {
    DB_CONNECTION_FAILED = 9001,          // 数据库连接失败
    DB_OPERATION_FAILED = 9002,           // 数据库操作失败
    DB_TRANSACTION_FAILED = 9003,         // 数据库事务失败
    DB_COMMIT_FAILED = 9004,              // 数据库提交失败
    INTERNAL_ERROR = 9005,                // 服务器内部错误
    SETTINGS_NOT_LOADED = 9006,           // 系统设置未加载
    SETTINGS_SAVE_FAILED = 9007,          // 系统设置保存失败
    SMTP_NOT_CONFIGURED = 9008,           // 邮件服务未配置
    REDIS_OPERATION_FAILED = 9009,        // 缓存服务操作失败
    METHOD_NOT_ALLOWED = 9010,            // 不支持的请求方法
    NOTIFICATION_SEND_FAILED = 9011,      // 通知发送失败
    AI_CONFIG_SAVE_FAILED = 9012,         // AI配置保存失败
    ARTICLE_PRAISE_FAILED = 9013,         // 点赞文章失败
    ARTICLE_UNPRAISE_FAILED = 9014,       // 取消点赞文章失败
    NOTIFICATION_TITLE_TOO_LONG = 9015,   // 通知标题过长
    NOTIFICATION_CONTENT_TOO_LONG = 9016, // 通知内容过长
    NOTIFICATION_USERS_REQUIRED = 9017,   // 缺少目标用户
    NOTIFICATION_INVALID_TARGET = 9018,   // 无效的通知目标类型
    SMTP_ENCRYPTION_UNSUPPORTED = 9019,   // 不支持的邮件加密方式（STARTTLS）
};

enum FeedBackError {
    FEEDBACK_NOT_FOUND = 10001, // 反馈不存在
};

enum FAQError {
    FAQ_NOT_FOUND = 11001, // 常见问题不存在
};

enum MessageError {
    MESSAGE_CONVERSATION_NOT_FOUND = 12001, // 会话不存在
    MESSAGE_SEND_FAILED = 12002,            // 消息发送失败
    MESSAGE_TEXT_TOO_LONG = 12003,          // 消息内容过长
    MESSAGE_PEER_NOT_FOUND = 12004,         // 对端用户不存在
    MESSAGE_CANNOT_SELF = 12005,            // 不能与自己私信
};
