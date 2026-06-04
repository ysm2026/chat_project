#ifndef NETPACKET_H
#define NETPACKET_H

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace NetPacket {

constexpr quint32 kMaxBodySize = 1024 * 1024;

QByteArray encode(const QString &line);
void feed(QByteArray &recvBuffer, const QByteArray &chunk, QStringList &completeLines);

} // namespace NetPacket

#endif // NETPACKET_H
