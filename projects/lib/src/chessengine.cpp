/*
    This file is part of Cute Chess.

    Cute Chess is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Cute Chess is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Cute Chess.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "chessengine.h"
#include <QIODevice>
#include <QTimer>
#include <QStringRef>
#include <QtDebug>
#include <QtAlgorithms>
#include "engineoption.h"


int ChessEngine::s_count = 0;

QStringRef ChessEngine::nextToken(const QStringRef& previous, bool untilEnd)
{
	const QString* str = previous.string();
	if (str == 0)
		return QStringRef();

	int i;
	int start = -1;
	int firstPos = previous.position() + previous.size();

	for (i = firstPos; i < str->size(); i++)
	{
		if (str->at(i).isSpace())
		{
			if (start == -1)
				continue;
			break;
		}
		else if (start == -1)
		{
			start = i;
			if (untilEnd)
			{
				int end = str->size();
				while (str->at(--end).isSpace());
				i = end + 1;
				break;
			}
		}
	}

	if (start == -1)
		return QStringRef();
	return QStringRef(str, start, i - start);
}

QStringRef ChessEngine::firstToken(const QString& str, bool untilEnd)
{
	return nextToken(QStringRef(&str, 0, 0), untilEnd);
}


ChessEngine::ChessEngine(QObject* parent)
	: ChessPlayer(parent),
	  m_whiteEvalPov(false),
	  m_id(s_count++),
	  m_pingState(NotStarted),
	  m_pinging(false),
	  m_pingTimer(new QTimer(this)),
	  m_quitTimer(new QTimer(this)),
	  m_idleTimer(new QTimer(this)),
	  m_ioDevice(0)
{
	m_pingTimer->setSingleShot(true);
	m_pingTimer->setInterval(10000);
	connect(m_pingTimer, SIGNAL(timeout()), this, SLOT(onPingTimeout()));

	m_quitTimer->setSingleShot(true);
	m_quitTimer->setInterval(2000);
	connect(m_quitTimer, SIGNAL(timeout()), this, SLOT(onQuitTimeout()));

	m_idleTimer->setSingleShot(true);
	m_idleTimer->setInterval(10000);
	connect(m_idleTimer, SIGNAL(timeout()), this, SLOT(onIdleTimeout()));
}

ChessEngine::~ChessEngine()
{
	qDeleteAll(m_options);
}

QIODevice* ChessEngine::device() const
{
	return m_ioDevice;
}

void ChessEngine::setDevice(QIODevice* device)
{
	Q_ASSERT(device != 0);

	m_ioDevice = device;
	connect(m_ioDevice, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
	connect(m_ioDevice, SIGNAL(readChannelFinished()), this, SLOT(onDisconnect()));
}

void ChessEngine::applyConfiguration(const EngineConfiguration& configuration)
{
	foreach (const QString& str, configuration.initStrings())
		write(str);

	foreach (EngineOption* option, configuration.options())
		setOption(option->name(), option->value());

	m_whiteEvalPov = configuration.whiteEvalPov();
}

EngineOption* ChessEngine::getOption(const QString& name) const
{
	foreach (EngineOption* option, m_options)
	{
		if (option->alias() == name || option->name() == name)
			return option;
	}

	return 0;
}

void ChessEngine::setOption(const QString& name, const QVariant& value)
{
	if (state() == Starting || state() == NotStarted)
	{
		m_optionBuffer[name] = value;
		return;
	}

	EngineOption* option = getOption(name);
	if (option == 0)
	{
		qDebug() << this->name() << "doesn't have option" << name;
		return;
	}

	if (!option->isValid(value))
	{
		qDebug() << "Invalid value for option" << name
			 << ":" << value.toString();
		return;
	}

	option->setValue(value);
	sendOption(option->name(), value.toString());
}

QList<EngineOption*> ChessEngine::options() const
{
	return m_options;
}

void ChessEngine::start()
{
	if (state() != NotStarted)
		return;
	
	m_pinging = false;
	setState(Starting);

	flushWriteBuffer();
	
	startProtocol();
	m_pinging = true;
}

void ChessEngine::onProtocolStart()
{
	m_pinging = false;
	setState(Idle);
	Q_ASSERT(isReady());

	flushWriteBuffer();

	QMap<QString, QVariant>::const_iterator i = m_optionBuffer.constBegin();
	while (i != m_optionBuffer.constEnd())
	{
		setOption(i.key(), i.value());
		++i;
	}
	m_optionBuffer.clear();
}

void ChessEngine::go()
{
	if (state() == Observing)
		ping();
	ChessPlayer::go();
}

void ChessEngine::endGame(const Chess::Result& result)
{
	ChessPlayer::endGame(result);
	ping();
}

bool ChessEngine::isHuman() const
{
	return false;
}

bool ChessEngine::isReady() const
{
	if (m_pinging)
		return false;
	return ChessPlayer::isReady();
}

bool ChessEngine::supportsVariant(const QString& variant) const
{
	return m_variants.contains(variant);
}

void ChessEngine::stopThinking()
{
	if (state() == Thinking && !m_pinging)
	{
		m_idleTimer->start();
		sendStop();
	}
}

void ChessEngine::onIdleTimeout()
{
	m_idleTimer->stop();
	if (state() != Thinking || m_pinging)
		return;

	m_writeBuffer.clear();
	closeConnection();

	emitForfeit(Chess::Result::StalledConnection);
}

void ChessEngine::closeConnection()
{
	if (state() == Disconnected)
		return;
	ChessPlayer::closeConnection();

	m_pinging = false;
	m_pingTimer->stop();
	m_writeBuffer.clear();
	emit ready();

	disconnect(m_ioDevice, SIGNAL(readChannelFinished()),
		   this, SLOT(onDisconnect()));
	m_ioDevice->close();
}

void ChessEngine::onTimeout()
{
	stopThinking();
}

void ChessEngine::ping()
{
	if (m_pinging
	||  state() == NotStarted
	||  state() == Disconnected
	||  !sendPing())
		return;

	m_pinging = true;
	m_pingState = state();
	m_pingTimer->start();
}

void ChessEngine::pong()
{
	if (!m_pinging)
		return;

	m_pingTimer->stop();
	m_pinging = false;
	flushWriteBuffer();

	if (state() == FinishingGame)
	{
		if (m_pingState == FinishingGame)
		{
			setState(Idle);
			m_pingState = Idle;
		}
		// If the status changed while waiting for a ping response, then
		// ping again to make sure that we can move on to the next game.
		else
		{
			ping();
			return;
		}
	}

	emit ready();
}

void ChessEngine::onPingTimeout()
{
	qDebug() << "Engine" << name() << "failed to respond to ping";

	m_pinging = false;
	m_writeBuffer.clear();
	closeConnection();

	emitForfeit(Chess::Result::StalledConnection);
}

void ChessEngine::write(const QString& data, WriteMode mode)
{
	if (state() == Disconnected)
		return;
	if (state() == NotStarted
	||  (m_pinging && mode == Buffered))
	{
		m_writeBuffer.append(data);
		return;
	}

	Q_ASSERT(m_ioDevice->isWritable());
	emit debugMessage(QString(">%1(%2): %3")
			  .arg(name())
			  .arg(m_id)
			  .arg(data));

	m_ioDevice->write(data.toAscii() + "\n");
}

void ChessEngine::onReadyRead()
{
	while (m_ioDevice->isReadable() && m_ioDevice->canReadLine())
	{
		m_idleTimer->stop();

		QString line = QString(m_ioDevice->readLine());
		if (line.endsWith('\n'))
			line.chop(1);
		if (line.endsWith('\r'))
			line.chop(1);
		if (line.isEmpty())
			continue;

		emit debugMessage(QString("<%1(%2): %3")
				  .arg(name())
				  .arg(m_id)
				  .arg(line));
		parseLine(line);
	}
}

void ChessEngine::flushWriteBuffer()
{
	if (m_pinging || state() == NotStarted)
		return;

	foreach (const QString& line, m_writeBuffer)
		write(line);
	m_writeBuffer.clear();
}

void ChessEngine::onQuitTimeout()
{
	Q_ASSERT(state() != Disconnected);

	disconnect(m_ioDevice, SIGNAL(readChannelFinished()), this, SLOT(onQuitTimeout()));

	if (!m_quitTimer->isActive())
		closeConnection();
	else
		m_quitTimer->stop();

	ChessPlayer::quit();
}

void ChessEngine::quit()
{
	if (!m_ioDevice || !m_ioDevice->isOpen() || state() == Disconnected)
		return ChessPlayer::quit();

	disconnect(m_ioDevice, SIGNAL(readChannelFinished()), this, SLOT(onDisconnect()));
	connect(m_ioDevice, SIGNAL(readChannelFinished()), this, SLOT(onQuitTimeout()));
	sendQuit();
	m_quitTimer->start();
}
