#include "tcpinterface.h"

#include <QDebug>

RemoteControlSocket::RemoteControlSocket(uint16_t port) : server() {
	if (!server.listen(QHostAddress::Any, port)) {
		qWarning() << "RCS: failed to listen on port" << port << "error:" << server.errorString();
	} else {
		qInfo() << "RCS: listening on" << server.serverAddress().toString() << "port" << server.serverPort();
	}
	connect(&server, &QTcpServer::newConnection, this, &RemoteControlSocket::addClient);
}

void RemoteControlSocket::addClient() {
	auto *client = server.nextPendingConnection();
	if (!client) {
		qWarning() << "RCS: newConnection fired but no pending client";
		return;
	}
	qInfo() << "RCS: client connected from" << client->peerAddress().toString() << ":" << client->peerPort();
	clients.push_back(client);
	connect(client, &QTcpSocket::disconnected, this, [this, client]() {
		qInfo() << "RCS: client disconnected" << client;
		clients.removeAll(client);
		client->deleteLater();
	});
	connect(client, &QTcpSocket::readyRead, this, [this, client]() {
		bool processedLine = false;
		while (client->canReadLine()) {
			processedLine = true;
			this->handleLine(client->readLine().trimmed(), client);
		}
		if (!processedLine) {
			QByteArray pending = client->peek(client->bytesAvailable());
			qWarning() << "RCS: readyRead but no full line yet; ensure command ends with newline. Pending bytes:" << pending;
		}
	});
}

void RemoteControlSocket::sendReply(QTcpSocket *sock, const QString &message) {
	if (!sock || sock->state() != QAbstractSocket::ConnectedState) {
		qWarning() << "RCS: attempted reply on invalid/disconnected socket";
		return;
	}
	QByteArray payload = message.toUtf8();
	if (!payload.endsWith('\n')) payload.append('\n');
	qInfo() << "RCS: sending reply:" << payload.trimmed();
	sock->write(payload);
	sock->flush();
}

void RemoteControlSocket::handleLine(QString s, QTcpSocket *sock) {
	qInfo() << "RCS: received command:" << s << "from" << sock;
	if (s == "start") {
		emit start();
		sendReply(sock, "OK");
	} else if (s == "stop") {
		emit stop();
		sendReply(sock, "OK");
	} else if (s == "update") {
		emit refresh_streams();
		sendReply(sock, "OK");
	} else if (s == "streams") {
		qInfo() << "RCS: handling streams command, emitting request_streams";
		emit request_streams(sock);
	} else if (s.startsWith("filename")) {
		emit filename(s);
		sendReply(sock, "OK");
	} else if (s == "select all") {
			emit select_all();
			sendReply(sock, "OK");
	} else if (s == "select none") {
			emit select_none();
			sendReply(sock, "OK");
	} else if (s.startsWith("select")) {
		QStringList streams;
		QRegularExpression re("\\{([^}]*)\\}");
		QRegularExpressionMatchIterator iterator = re.globalMatch(s);
		while (iterator.hasNext()) {
			auto match = iterator.next();
			QString stream = match.captured(1).trimmed();
			if (!stream.isEmpty()) streams << stream;
		}

		if (streams.isEmpty())
			sendReply(sock, "ERR No streams specified");
		else
			emit select_streams(sock, streams);
	} else {
		sendReply(sock, "ERR Unknown command");
	}
}
