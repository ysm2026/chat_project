#ifndef DBCONFIG_H
#define DBCONFIG_H

#include <QtGlobal>

namespace DbConfig {

// TCP 聊天服务（客户端连接地址、服务端监听端口）
inline constexpr const char *kChatHost = "127.0.0.1";
inline constexpr quint16 kChatPort = 8888;

// MySQL
inline constexpr const char *kHost = "localhost";
inline constexpr int kPort = 3306;
inline constexpr const char *kDatabaseName = "chat_room";
inline constexpr const char *kUser = "root";
inline constexpr const char *kPassword = "663399QQ";
inline constexpr const char *kSslMode = "DISABLED";     // 需要 SSL 时可改为 "PREFERRED"

} // namespace DbConfig

#endif // DBCONFIG_H
