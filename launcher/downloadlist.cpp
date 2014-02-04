/*
 *  Copyright (C) 2005-2006 Alo Sarv <madcat_@users.sourceforge.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "downloadlist.h"
#include <QHeaderView>
#include <hncgcomm/cgcomm.h>

DownloadList::DownloadList(QWidget *parent) : QTreeWidget(parent) {}
void DownloadList::init() {
	QStringList downloadHeaders;
	downloadHeaders << "File Name" << "File Size" << "Availability";
	downloadHeaders << "Speed" << "Progress";
	setHeaderLabels(downloadHeaders);
	header()->setStretchLastSection(false);
}

void DownloadList::resizeEvent(QResizeEvent *event) {
	int nameCol = 0;
	QList<int> toResize;
	if (headerItem()->text(1) == "File Name") {
		nameCol = 1;
		toResize << 0 << 2 << 3 << 4;
	} else if (headerItem()->text(2) == "File Name") {
		nameCol = 2;
		toResize << 0 << 1 << 3 << 4;
	} else if (headerItem()->text(3) == "File Name") {
		nameCol = 3;
		toResize << 0 << 1 << 2 << 4;
	} else if (headerItem()->text(4) == "File Name") {
		nameCol = 4;
		toResize << 0 << 1 << 2 << 3;
	} else {
		toResize << 1 << 2 << 3 << 4;
	}
	foreach(int i, toResize) {
		header()->resizeSection(i, 100);
	}
	header()->resizeSection(nameCol, viewport()->width() - 400);
}

void DownloadList::setSource(Engine::DownloadList *source) {
	Q_ASSERT(source);
	m_source.reset(source);
	m_source->getList();
	m_source->monitor(250);
}

void DownloadList::updateList(
	const std::vector<Engine::DownloadInfoPtr> &data, bool complete
){
	if (complete) {
		clear();
		foreach(Engine::DownloadInfoPtr obj, data) {
			new DownloadListItem(this, obj);
		}
	} else {
		foreach(Engine::DownloadInfoPtr obj, data) {
			Iter i = m_entries.find(obj->getId());
			if (i == m_entries.end()) {
				new DownloadListItem(this, obj);
			} else {
				(*i)->update(obj);
			}
		}
	}
}

DownloadListItem::DownloadListItem(
	QTreeWidget *parent, Engine::DownloadInfoPtr data
) : QTreeWidgetItem(parent), m_data(data) {
	DownloadList *list = dynamic_cast<DownloadList*>(parent);
	Q_ASSERT(list);
	list->m_entries[data->getId()] = this;
	setText(0, QString::fromStdString(data->getName()));
	setText(1, QString::number(data->getSize()));
	setText(2, QString::number(data->getSourceCnt()));
	setText(4, QString::number(
		data->getCompleted() * 100.0 / data->getSize(), 'g', 2) + "%"
	);
	QIcon i(":/icons/progressicon1.png");
	setIcon(0, i);
}

DownloadListItem::~DownloadListItem() {
	DownloadList *list = dynamic_cast<DownloadList*>(parent());
	Q_ASSERT(list);
	list->m_entries.remove(m_data->getId());
}

void DownloadListItem::update(Engine::DownloadInfoPtr data) {
	Q_ASSERT(data->getId() == m_data->getId());
	if (text(0) != QString::fromStdString(data->getName())) {
		setText(0, QString::fromStdString(data->getName()));
	}
	if (text(1) != QString::number(data->getSize())) {
		setText(1, QString::number(data->getSize()));
	}
	if (text(2) != QString::number(data->getSourceCnt())) {
		setText(2, QString::number(data->getSourceCnt()));
	}
	float perc = data->getCompleted() * 100.0 / data->getSize();
	if (text(4) != QString::number(perc)) {
		setText(4, QString::number(perc, 'g', 2) + "%");
	}
	m_data = data;
}
