/*
    This file is part of SloppyGUI.

    SloppyGUI is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    SloppyGUI is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with SloppyGUI.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QPainter>
#include <QSvgRenderer>
#include <QGraphicsSvgItem>

#include "graphicschessboarditem.h"
#include "graphicschessboardsquareitem.h"

const qreal GraphicsChessboardItem::size = 400;
const qreal GraphicsChessboardItem::borderSize = 25;

GraphicsChessboardItem::GraphicsChessboardItem(QGraphicsItem* parent)
	: QGraphicsItem(parent)
{
	m_showBorder = true;
	m_borderColor = QColor("peru");

	bool colorToggle = false;
	GraphicsChessboardSquareItem* square;

	for (int i = 0; i < 8; i++)
	{
		colorToggle = !colorToggle;

		for (int j = 0; j < 8; j++)
		{
			if (colorToggle)
			{
				square = new GraphicsChessboardSquareItem(this,
					GraphicsChessboardSquareItem::LightSquare);
			}
			else
			{
				square = new GraphicsChessboardSquareItem(this,
					GraphicsChessboardSquareItem::DarkSquare);
			}
			
			if (isBorderVisible())
			{
				square->setPos(GraphicsChessboardSquareItem::size * j +
					GraphicsChessboardItem::borderSize,
					GraphicsChessboardSquareItem::size * i +
					GraphicsChessboardItem::borderSize);
			}
			else
			{
				square->setPos(GraphicsChessboardSquareItem::size * j,
					GraphicsChessboardSquareItem::size * i);
			}

			m_squares[i][j] = square;

			colorToggle = !colorToggle;
		}
	}
}

GraphicsChessboardSquareItem* GraphicsChessboardItem::squareAt(int file, int rank)
{
	Q_ASSERT_X(file >= 0 && file <= 7, "squareAt(int, int)", "invalid file (column) number");
	Q_ASSERT_X(rank >= 0 && rank <= 7, "squareAt(int, int)", "invalid rank (row) number");

	return m_squares[rank][file];
}

GraphicsChessboardSquareItem* GraphicsChessboardItem::squareAt(const QChar& file, const QChar& rank)
{
	int fileNum = -1;
	
	if (file == QChar('a'))
		fileNum = 0;
	else if (file == QChar('b'))
		fileNum = 1;
	else if (file == QChar('c'))
		fileNum = 2;
	else if (file == QChar('d'))
		fileNum = 3;
	else if (file == QChar('e'))
		fileNum = 4;
	else if (file == QChar('f'))
		fileNum = 5;
	else if (file == QChar('g'))
		fileNum = 6;
	else if (file == QChar('h'))
		fileNum = 7;

	// Returns valid square or hits squareAt(int, int)'s asserts
	return squareAt(fileNum, 8 - rank.digitValue());
}

QRectF GraphicsChessboardItem::boundingRect() const
{
	if (isBorderVisible())
	{
		return QRectF(0, 0, GraphicsChessboardItem::size +
			(2 * GraphicsChessboardItem::borderSize), GraphicsChessboardItem::size +
			(2 * GraphicsChessboardItem::borderSize));
	}

	return QRectF(0, 0, GraphicsChessboardItem::size, GraphicsChessboardItem::size);
}

void GraphicsChessboardItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	Q_UNUSED(option)
	Q_UNUSED(widget)

	painter->fillRect(boundingRect(), m_borderColor);
}

void GraphicsChessboardItem::showBorder(bool visibility)
{
	m_showBorder = visibility;
}

bool GraphicsChessboardItem::isBorderVisible() const
{
	return m_showBorder;
}
