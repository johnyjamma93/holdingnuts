/*
 * Copyright 2008, 2009, Dominik Geyer
 *
 * This file is part of HoldingNuts.
 *
 * HoldingNuts is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HoldingNuts is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HoldingNuts.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *     Michael Miller <michael.miller@holdingnuts.net>
 */


#ifndef _HOLDING_NUTS_CHIP_STAKE_H
#define _HOLDING_NUTS_CHIP_STAKE_H

#include "Player.hpp"

#include <QGraphicsItem>

class ChipStake : public QObject, public QGraphicsItem
{
	Q_OBJECT

public:
	ChipStake(QGraphicsItem* parent = 0);

	QRectF boundingRect() const;

	void paint(
		QPainter* painter,
		const QStyleOptionGraphicsItem* option,
		QWidget* widget);
		
	void setAmount(chips_type amount);
	
	void clear();

private:
	void addChips(unsigned num, const QColor& c, qreal& y, qreal& z);
	void addJetons(unsigned num, const QColor& c, qreal& y, qreal& z);
};

#endif /* _HOLDING_NUTS_CHIP_STAKE_H */
