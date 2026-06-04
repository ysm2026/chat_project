#include "netpacket.h"

#include <QtEndian>
#include <cstring>

namespace NetPacket {

QByteArray encode(const QString &line)
{
    const QByteArray body = line.toUtf8();
    const quint32 len = static_cast<quint32>(body.size());

    QByteArray packet;
    packet.resize(4 + body.size());
    qToBigEndian(len, packet.data());
    if (!body.isEmpty()) {
        memcpy(packet.data() + 4, body.constData(), static_cast<size_t>(body.size()));
    }
    return packet;
}

void feed(QByteArray &recvBuffer, const QByteArray &chunk, QStringList &completeLines)
{
    if (!chunk.isEmpty()) {
        recvBuffer.append(chunk);
    }

    while (recvBuffer.size() >= 4) {
        quint32 bodyLen = 0;
        memcpy(&bodyLen, recvBuffer.constData(), 4);
        bodyLen = qFromBigEndian(bodyLen);

        if (bodyLen == 0 || bodyLen > kMaxBodySize) {
            recvBuffer.clear();
            break;
        }

        const int frameSize = 4 + static_cast<int>(bodyLen);
        if (recvBuffer.size() < frameSize) {
            break;
        }

        const QByteArray body = recvBuffer.mid(4, static_cast<int>(bodyLen));
        recvBuffer.remove(0, frameSize);
        completeLines.append(QString::fromUtf8(body));
    }
}

} // namespace NetPacket