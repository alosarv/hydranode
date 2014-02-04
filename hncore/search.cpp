/*
 *  Copyright (C) 2004-2006 Alo Sarv <madcat_@users.sourceforge.net>
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

/**
 * \file search.cpp Implementation of Hydranode Search API
 */

#include <hncore/pch.h>
#include <hnbase/log.h>
#include <hncore/search.h>
#include <boost/tokenizer.hpp>

// SearchResult class
// ------------------
SearchResult::SearchResult(const std::string &filename, uint64_t fileSize)
: m_fileName(filename), m_fileSize(fileSize), m_sourceCnt(), m_completeCnt(),
m_rating() {}
SearchResult::~SearchResult() {}

// Search class
// ------------
boost::signal<void (SearchPtr)> Search::s_sigQuery;
std::vector<LinkHandler> Search::s_linkHandlers;
std::vector<FileHandler> Search::s_fileHandlers;

Search::Search() 
: m_minSize(), m_maxSize(std::numeric_limits<uint64_t>::max()),
m_type(FT_UNKNOWN), m_running() {}

Search::Search(const std::string &terms) : m_minSize(),
m_maxSize(std::numeric_limits<uint64_t>::max()), m_type(FT_UNKNOWN),
m_running() {
	boost::tokenizer<> tok(terms);
	for (boost::tokenizer<>::iterator i = tok.begin(); i != tok.end(); ++i){
		m_terms.push_back(*i);
	}
}

void Search::addQueryHandler(SearchHandler handler) {
	s_sigQuery.connect(handler);
}

void Search::run() {
	m_running = true;
	s_sigQuery(shared_from_this());
}

void Search::stop() {
	m_running = false;
}

void Search::addResult(SearchResultPtr result) {
	if (result->getSize() > getMaxSize()) {
		return;
	}
	if (result->getSize() < getMinSize()) {
		return;
	}
	IDContainer::iterator it = m_resultsById.find(result->identifier());
	if (it != m_resultsById.end()) {
		(*it)->addSources(result->getSources());
		(*it)->addComplete(result->getComplete());
		(*it)->addRating(result->getRating());
		(*it)->addName(result->getName());
		SearchResult::CSIter j = result->sbegin();
		while (j != result->send()) {
			(*it)->addSource(*j++);
		}
	} else {
		m_resultsById.insert(result);
		m_results.push_back(result);
	}
}

void Search::notifyResults() {
	m_sigResult(shared_from_this());
}

void Search::addLinkHandler(LinkHandler handler) {
	s_linkHandlers.push_back(handler);
}

void Search::addFileHandler(FileHandler handler) {
	s_fileHandlers.push_back(handler);
}

bool Search::downloadLink(const std::string &link) {
	std::vector<LinkHandler>::iterator i = s_linkHandlers.begin();
	while (i != s_linkHandlers.end()) {
		if ((*i++)(link)) {
			return true;
		}
	}
	return false;
}

bool Search::downloadFile(const std::string &contents) {
	std::vector<FileHandler>::iterator i = s_fileHandlers.begin();
	while (i != s_fileHandlers.end()) {
		if ((*i++)(contents)) {
			return true;
		}
	}
	return false;
}

SearchResultPtr Search::getResult(uint32_t num) const {
	return m_results.at(num);
}

SearchResultPtr Search::getResult(const std::string &id) const {
	IDContainer::const_iterator it(m_resultsById.find(id));
	if (it != m_resultsById.end()) {
		return *it;
	} 
	return SearchResultPtr();
}
