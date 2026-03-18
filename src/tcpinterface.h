#pragma once

#include <cstdint>

#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QDataStream>
#include <QStringList>
#include <qregularexpression.h>

class RemoteControlSocket : public QObject {
	Q_OBJECT
	QTcpServer server;
	QList<QTcpSocket*> clients;
public:
	RemoteControlSocket(uint16_t port);

signals:
	void refresh_streams();
	void start();
	void stop();
	void filename(QString s);
	void request_streams(QTcpSocket *sock);
	void select_streams(QTcpSocket *sock, QStringList streams);
	void select_all();
	void select_none();

public slots:
	void addClient();
	void handleLine(QString s, QTcpSocket* sock);
	void sendReply(QTcpSocket *sock, const QString &message);
};
