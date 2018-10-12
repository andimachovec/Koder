/*
 * Copyright 2014-2018 Kacper Kasper <kacperkasper@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "Editor.h"

#include <Messenger.h>

#include <algorithm>
#include <string>

#include "Preferences.h"
#include "ScintillaUtils.h"
#include "StatusView.h"


namespace Sci = Scintilla;
using namespace Sci::Properties;


Editor::Editor()
	:
	BScintillaView("EditorView", 0, true, true, B_NO_BORDER),
	fCommentLineToken(""),
	fCommentBlockStartToken(""),
	fCommentBlockEndToken(""),
	fHighlightedWhitespaceStart(0),
	fHighlightedWhitespaceEnd(0),
	fHighlightedWhitespaceCurrentPos(0),
	fSearchTarget(-1, -1),
	fSearchLastResult(-1, -1),
	fSearchLast(""),
	fSearchLastFlags(0),
	fIncrementalSearch(false),
	fType(""),
	fReadOnly(false)
{
	fStatusView = new StatusView(this);
	AddChild(fStatusView);
}


void
Editor::DoLayout()
{
	BScintillaView::DoLayout();
	fStatusView->ResizeToPreferred();
}


void
Editor::FrameResized(float width, float height)
{
	BScintillaView::FrameResized(width, height);
	fStatusView->ResizeToPreferred();
}


void
Editor::NotificationReceived(SCNotification* notification)
{
	BMessenger window_msg(nullptr, (BLooper*) Window());
	switch(notification->nmhdr.code) {
		case SCN_SAVEPOINTLEFT:
			window_msg.SendMessage(EDITOR_SAVEPOINT_LEFT);
		break;
		case SCN_SAVEPOINTREACHED:
			window_msg.SendMessage(EDITOR_SAVEPOINT_REACHED);
		break;
		case SCN_MODIFIED:
			window_msg.SendMessage(EDITOR_MODIFIED);
		break;
		case SCN_CHARADDED: {
			char ch = static_cast<char>(notification->ch);
			_MaintainIndentation(ch);
		} break;
		case SCN_UPDATEUI:
			_BraceHighlight();
			_UpdateLineNumberWidth();
			_UpdateStatusView();
			window_msg.SendMessage(EDITOR_UPDATEUI);
		break;
		case SCN_MARGINCLICK:
			_MarginClick(notification->margin, notification->position);
		break;
	}
}


void
Editor::ContextMenu(BPoint point)
{
	BMessenger msgr(nullptr, (BLooper*) Window());
	BMessage msg(EDITOR_CONTEXT_MENU);
	msg.AddPoint("where", point);
	msgr.SendMessage(&msg);
}


void
Editor::SetPreferences(Preferences* preferences)
{
	fPreferences = preferences;
}


void
Editor::SetType(std::string type)
{
	fType = type;
	_UpdateStatusView();
}


void
Editor::SetRef(const entry_ref& ref)
{
	fStatusView->SetRef(ref);
	_UpdateStatusView();
}


void
Editor::SetReadOnly(bool readOnly)
{
	fReadOnly = readOnly;
	SendMessage(SCI_SETREADONLY, fReadOnly, 0);
	_UpdateStatusView();
}


void
Editor::CommentLine(Scintilla::Range range)
{
	const auto start = range.first;
	const auto end = range.second;
	if(end < start) return;

	Sci::Guard<SearchTarget> guard(this);
	Sci::UndoAction action(this);

	int startLine = SendMessage(SCI_LINEFROMPOSITION, start, 0);
	int endLine = SendMessage(SCI_LINEFROMPOSITION, end, 0);
	Sci_Position lineStartPos = SendMessage(SCI_POSITIONFROMLINE, startLine, 0);
	const size_t tokenLength = fCommentLineToken.length();
	const char* token = fCommentLineToken.c_str();

	// check for comment tokens at the beggining of the lines
	Set<SearchTarget>({lineStartPos, end});
	Sci_Position pos = SendMessage(SCI_SEARCHINTARGET, (uptr_t) tokenLength, (sptr_t) token);
	// check only the first line here, so fragments with one line comments can
	// be commented
	Sci_Position maxPos = SendMessage(SCI_GETLINEINDENTPOSITION, startLine, 0);
	if(pos != -1 && pos <= maxPos) {
		int charactersRemoved = 0;
		int line = startLine;
		while(pos != -1) {
			Set<SearchTarget>({SendMessage(SCI_POSITIONFROMLINE, line, 0), end - charactersRemoved});
			pos = SendMessage(SCI_SEARCHINTARGET, (uptr_t) tokenLength, (sptr_t) token);
			maxPos = SendMessage(SCI_GETLINEINDENTPOSITION, line, 0);
			line++;
			if(pos != -1 && pos <= maxPos) {
				SendMessage(SCI_REPLACETARGET, 0, (sptr_t) "");
				charactersRemoved += 2;
			}
		}
	} else {
		int addedCharacters = 0;
		while(startLine <= endLine) {
			Sci_Position linePos = SendMessage(SCI_POSITIONFROMLINE, startLine, 0);
			SendMessage(SCI_INSERTTEXT, linePos, (sptr_t) token);
			addedCharacters += tokenLength;
			startLine++;
		}
		Set<Selection>({start + tokenLength, end + addedCharacters});
	}
}


void
Editor::CommentBlock(Scintilla::Range range)
{
	const auto start = range.first;
	const auto end = range.second;
	if(start == end || end < start) return;

	const size_t startTokenLen = fCommentBlockStartToken.length();
	const size_t endTokenLen = fCommentBlockEndToken.length();
	bool startTokenPresent = true;
	bool endTokenPresent = true;
	for(int i = 0; i < startTokenLen; i++) {
		if(SendMessage(SCI_GETCHARAT, start + i, 0) != fCommentBlockStartToken[i])
			startTokenPresent = false;
	}
	for(int i = 0; i < endTokenLen; i++) {
		if(SendMessage(SCI_GETCHARAT, end - endTokenLen + i, 0) != fCommentBlockEndToken[i])
			endTokenPresent = false;
	}

	Sci::UndoAction action(this);
	if(startTokenPresent && endTokenPresent) {
		// order is important here
		SendMessage(SCI_DELETERANGE, end - endTokenLen, endTokenLen);
		SendMessage(SCI_DELETERANGE, start, startTokenLen);
		Set<Selection>({start, end - startTokenLen - endTokenLen});
	} else {
		SendMessage(SCI_INSERTTEXT, start, (sptr_t) fCommentBlockStartToken.c_str());
		SendMessage(SCI_INSERTTEXT, end + startTokenLen, (sptr_t) fCommentBlockEndToken.c_str());
		Set<Selection>({start, end + startTokenLen + endTokenLen});
	}
}


void
Editor::SetCommentLineToken(std::string token)
{
	fCommentLineToken = token;
}


void
Editor::SetCommentBlockTokens(std::string start, std::string end)
{
	fCommentBlockStartToken = start;
	fCommentBlockEndToken = end;
}


bool
Editor::CanCommentLine()
{
	return fCommentLineToken != "";
}


bool
Editor::CanCommentBlock()
{
	return fCommentBlockStartToken != "" && fCommentBlockEndToken != "";
}


void
Editor::HighlightTrailingWhitespace()
{
	int length = SendMessage(SCI_GETLENGTH, 0, 0);
	int firstVisibleLine = SendMessage(SCI_GETFIRSTVISIBLELINE, 0, 0) - 2;
	int firstLine = std::max(firstVisibleLine, 0);
	int lastLine = firstLine + SendMessage(SCI_LINESONSCREEN, 0, 0) + 3;
	int firstLinePos = SendMessage(SCI_POSITIONFROMLINE, firstLine, 0);
	// SCI_POSITIONFROMLINE returns -1 if line is above range
	int lastLinePos = SendMessage(SCI_POSITIONFROMLINE, lastLine, 0);
	int lastLineEndPos = std::max(lastLinePos, length);
	_HighlightTrailingWhitespace(firstLinePos, lastLineEndPos);
}


void
Editor::ClearHighlightedWhitespace()
{
	Sci::Guard<CurrentIndicator> guard(this);

	SendMessage(SCI_SETINDICATORCURRENT, Indicator::WHITESPACE, 0);

	// cleanup after previous runs
	SendMessage(SCI_INDICATORCLEARRANGE, fHighlightedWhitespaceStart,
		fHighlightedWhitespaceEnd - fHighlightedWhitespaceStart);

	fHighlightedWhitespaceStart = 0;
	fHighlightedWhitespaceEnd = 0;
	fHighlightedWhitespaceCurrentPos = 0;
}


void
Editor::TrimTrailingWhitespace()
{
	Sci::Guard<SearchTarget, SearchFlags> guard(this);

	Sci_Position length = SendMessage(SCI_GETLENGTH, 0, 0);
	Set<SearchTarget>({0, length});
	Set<SearchFlags>(SCFIND_REGEXP | SCFIND_CXX11REGEX);

	Sci::UndoAction action(this);
	const std::string whitespace = "\\s+$";
	int result;
	do {
		result = SendMessage(SCI_SEARCHINTARGET, whitespace.size(), (sptr_t) whitespace.c_str());
		if(result != -1) {
			SendMessage(SCI_REPLACETARGET, -1, (sptr_t) "");

			Set<SearchTarget>({Get<SearchTargetEnd>(), length});
		}
	} while(result != -1);
}


bool
Editor::Find(BMessage* message)
{
	bool inSelection = message->GetBool("inSelection");
	bool matchCase = message->GetBool("matchCase");
	bool matchWord = message->GetBool("matchWord");
	bool wrapAround = message->GetBool("wrapAround");
	bool backwards = message->GetBool("backwards");
	bool regex = message->GetBool("regex");
	const char* search = message->GetString("findText", "");

	Sci::Guard<SearchTarget, SearchFlags> guard(this);

	int length = SendMessage(SCI_GETLENGTH);
	Sci_Position anchor = SendMessage(SCI_GETANCHOR);
	Sci_Position current = SendMessage(SCI_GETCURRENTPOS);

	if(anchor != fSearchLastResult.first || current != fSearchLastResult.second) {
		ResetFindReplace();
	}

	if(fNewSearch == true) {
		if(inSelection == true) {
			fSearchTarget = Get<Selection>();
			if(backwards == true) {
				std::swap(fSearchTarget.first, fSearchTarget.second);
			}
		} else {
			fSearchTarget = backwards ? Sci::Range(anchor, 0) : Sci::Range(current, length);
		}
	}

	auto temp = fSearchTarget;

	if(fNewSearch == false) {
		temp.first = (backwards ? anchor : current);
	}

	bool found;
	found = _Find(search, temp.first, temp.second, matchCase, matchWord, regex);

	if(found == false && wrapAround == true) {
		Sci_Position startAgain;
		if(inSelection == true) {
			startAgain = fSearchTarget.first;
		} else {
			startAgain = (backwards ? length : 0);
		}
		found = _Find(search, startAgain, fSearchTarget.second, matchCase,
			matchWord, regex);
	}
	fNewSearch = false;
	fSearchLastMessage = *message;
	return found;
}


void
Editor::FindNext()
{
	Find(&fSearchLastMessage);
}


void
Editor::FindSelection()
{
	if(SendMessage(SCI_GETSELECTIONEMPTY) == false) {
		int length = SendMessage(SCI_GETSELTEXT);
		std::string selection(length, '\0');
		SendMessage(SCI_GETSELTEXT, 0, (sptr_t) &selection[0]);
		fSearchLastMessage.MakeEmpty();
		fSearchLastMessage.AddBool("wrapAround", true);
		fSearchLastMessage.AddString("findText", selection.c_str());
		Find(&fSearchLastMessage);
	}
}


void
Editor::Replace(std::string replacement, bool regex)
{
	Sci::Guard<SearchTarget, SearchFlags> guard(this);

	int replaceMsg = (regex ? SCI_REPLACETARGETRE : SCI_REPLACETARGET);
	if(fSearchLastResult != Sci::Range{ -1, -1 }) {
		// we need to search again, because whitespace highlighting messes with
		// the results
		Set<SearchFlags>(fSearchLastFlags);
		Set<SearchTarget>(fSearchLastResult);
		SendMessage(SCI_SEARCHINTARGET, (uptr_t) fSearchLast.size(), (sptr_t) fSearchLast.c_str());
		SendMessage(replaceMsg, -1, (sptr_t) replacement.c_str());
		fSearchLastResult = { -1, -1 };
	}
}


int
Editor::ReplaceAll(std::string search, std::string replacement, bool matchCase,
	bool matchWord, bool inSelection, bool regex)
{
	Sci::Guard<SearchTarget, SearchFlags> guard(this);
	Sci::UndoAction action(this);

	int replaceMsg = (regex ? SCI_REPLACETARGETRE : SCI_REPLACETARGET);
	int occurences = 0;
	SendMessage(inSelection ? SCI_TARGETFROMSELECTION : SCI_TARGETWHOLEDOCUMENT);
	auto target = Get<SearchTarget>();
	bool found;
	do {
		found = _Find(search, target.first, target.second, matchCase, matchWord, regex);
		if(found) {
			SendMessage(replaceMsg, -1, (sptr_t) replacement.c_str());
			target.first = Get<SearchTargetEnd>();
			occurences++;
		}
	} while(found);
	return occurences;
}


void
Editor::ReplaceAndFind()
{
	bool regex = fSearchLastMessage.GetBool("regex");
	const char* replaceText = fSearchLastMessage.GetString("replaceText", "");
	Replace(replaceText, regex);
	Find(&fSearchLastMessage);
}


void
Editor::ResetFindReplace()
{
	fNewSearch = true;
}


void
Editor::IncrementalSearch(std::string term)
{
	Sci::Guard<SearchTarget, SearchFlags> guard(this);

	int length = SendMessage(SCI_GETLENGTH);
	Sci_Position anchor = SendMessage(SCI_GETANCHOR);
	Sci_Position current = SendMessage(SCI_GETCURRENTPOS);

	if(fIncrementalSearch == false) {
		fIncrementalSearch = true;
		fSavedSelection = { anchor, current };
	}

	Sci_Position start = (anchor < current) ? anchor : current;
	bool found;
	found = _Find(term, start, length, false, false, false);

	if(found == false) {
		found = _Find(term, 0, start, false, false, false);
	}
	// nothing found
	if(found == false) {
		Set<Selection>(fSavedSelection);
	}
}


void
Editor::IncrementalSearchCancel()
{
	fIncrementalSearch = false;
	Set<Selection>(fSavedSelection);
}


void
Editor::IncrementalSearchCommit(std::string term)
{
	fIncrementalSearch = false;
	fSearchLastMessage.MakeEmpty();
	fSearchLastMessage.AddBool("wrapAround", true);
	fSearchLastMessage.AddString("findText", term.c_str());
}


// borrowed from SciTE
// Copyright (c) Neil Hodgson
void
Editor::_MaintainIndentation(char ch)
{
	int eolMode = SendMessage(SCI_GETEOLMODE, 0, 0);
	int currentLine = SendMessage(SCI_LINEFROMPOSITION, SendMessage(SCI_GETCURRENTPOS, 0, 0), 0);
	int lastLine = currentLine - 1;

	if(((eolMode == SC_EOL_CRLF || eolMode == SC_EOL_LF) && ch == '\n') ||
		(eolMode == SC_EOL_CR && ch == '\r')) {
		int indentAmount = 0;
		if(lastLine >= 0) {
			indentAmount = SendMessage(SCI_GETLINEINDENTATION, lastLine, 0);
		}
		if(indentAmount > 0) {
			_SetLineIndentation(currentLine, indentAmount);
		}
	}
}


void
Editor::_UpdateLineNumberWidth()
{
	if(fPreferences->fLineNumbers) {
		int numLines = SendMessage(SCI_GETLINECOUNT, 0, 0);
		int i;
		for(i = 1; numLines > 0; numLines /= 10, ++i);
		int charWidth = SendMessage(SCI_TEXTWIDTH, STYLE_LINENUMBER, (sptr_t) "0");
		SendMessage(SCI_SETMARGINWIDTHN, Margin::NUMBER, std::max(i, 3) * charWidth);
	}
}


void
Editor::_UpdateStatusView()
{
	Sci_Position pos = SendMessage(SCI_GETCURRENTPOS, 0, 0);
	int line = SendMessage(SCI_LINEFROMPOSITION, pos, 0);
	int column = SendMessage(SCI_GETCOLUMN, pos, 0);
	BMessage update(StatusView::UPDATE_STATUS);
	update.AddInt32("line", line + 1);
	update.AddInt32("column", column + 1);
	update.AddString("type", fType.c_str());
	update.AddBool("readOnly", fReadOnly);
	fStatusView->SetStatus(&update);
}


void
Editor::_BraceHighlight()
{
	if(fPreferences->fBracesHighlighting == true) {
		Sci_Position pos = SendMessage(SCI_GETCURRENTPOS, 0, 0);
		// highlight indent guide
		int line = SendMessage(SCI_LINEFROMPOSITION, pos, 0);
		int indentation = SendMessage(SCI_GETLINEINDENTATION, line, 0);
		SendMessage(SCI_SETHIGHLIGHTGUIDE, indentation, 0);
		// highlight braces
		if(_BraceMatch(pos - 1) == false) {
			_BraceMatch(pos);
		}
	} else {
		SendMessage(SCI_BRACEBADLIGHT, -1, 0);
	}
}


bool
Editor::_BraceMatch(int pos)
{
	char ch = SendMessage(SCI_GETCHARAT, pos, 0);
	if(ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == '{' || ch == '}') {
		int match = SendMessage(SCI_BRACEMATCH, pos, 0);
		if(match == -1) {
			SendMessage(SCI_BRACEBADLIGHT, pos, 0);
		} else {
			SendMessage(SCI_BRACEHIGHLIGHT, pos, match);
		}
	} else {
		SendMessage(SCI_BRACEBADLIGHT, -1, 0);
		return false;
	}
	return true;
}


void
Editor::_MarginClick(int margin, int pos)
{
	switch(margin) {
		case Margin::FOLD: {
			int lineNumber = SendMessage(SCI_LINEFROMPOSITION, pos, 0);
			SendMessage(SCI_TOGGLEFOLD, lineNumber, 0);
		} break;
	}
}


void
Editor::_HighlightTrailingWhitespace(Sci_Position start, Sci_Position end)
{
	if(start == end || end < start) return;

	int currentPos = SendMessage(SCI_GETCURRENTPOS, 0, 0);
	// don't thrash the CPU
	if(fHighlightedWhitespaceStart == start && fHighlightedWhitespaceEnd == end
			&& fHighlightedWhitespaceCurrentPos == currentPos) {
		return;
	}

	Sci::Guard<SearchTarget, SearchFlags, CurrentIndicator> guard(this);

	Set<SearchTarget>({start, end});
	Set<SearchFlags>(SCFIND_REGEXP | SCFIND_CXX11REGEX);

	Set<CurrentIndicator>(Indicator::WHITESPACE);

	// cleanup after previous runs
	SendMessage(SCI_INDICATORCLEARRANGE, fHighlightedWhitespaceStart,
		fHighlightedWhitespaceEnd - fHighlightedWhitespaceStart);

	const std::string whitespace = "\\s+$";
	int result;
	do {
		result = SendMessage(SCI_SEARCHINTARGET, whitespace.size(), (sptr_t) whitespace.c_str());
		if(result != -1) {
			auto found = Get<SearchTarget>();

			SendMessage(SCI_INDICATORFILLRANGE, found.first, found.second - found.first);

			Set<SearchTarget>({found.second + 1, end});
		}
	} while(result != -1);

	fHighlightedWhitespaceStart = start;
	fHighlightedWhitespaceEnd = end;
	fHighlightedWhitespaceCurrentPos = currentPos;
}


// borrowed from SciTE
// Copyright (c) Neil Hodgson
void
Editor::_SetLineIndentation(int line, int indent)
{
	if(indent < 0)
		return;

	Sci_CharacterRange crange;
	std::tie(crange.cpMin, crange.cpMax) = Get<Selection>();
	Sci_CharacterRange crangeStart = crange;
	int posBefore = SendMessage(SCI_GETLINEINDENTPOSITION, line, 0);
	SendMessage(SCI_SETLINEINDENTATION, line, indent);
	int posAfter = SendMessage(SCI_GETLINEINDENTPOSITION, line, 0);
	int posDifference = posAfter - posBefore;
	if(posAfter > posBefore) {
		if(crange.cpMin >= posBefore) {
			crange.cpMin += posDifference;
		}
		if(crange.cpMax >= posBefore) {
			crange.cpMax += posDifference;
		}
	} else if(posAfter < posBefore) {
		if(crange.cpMin >= posAfter) {
			if(crange.cpMin >= posBefore) {
				crange.cpMin += posDifference;
			} else {
				crange.cpMin = posAfter;
			}
		}
		if(crange.cpMax >= posAfter) {
			if(crange.cpMax >= posBefore) {
				crange.cpMax += posDifference;
			} else {
				crange.cpMax = posAfter;
			}
		}
	}
	if((crangeStart.cpMin != crange.cpMin) || (crangeStart.cpMax != crange.cpMax)) {
		Set<Selection>({static_cast<int>(crange.cpMin), static_cast<int>(crange.cpMax)});
	}
}


bool
Editor::_Find(std::string search, Sci_Position start, Sci_Position end,
	bool matchCase, bool matchWord, bool regex)
{
	int searchFlags = 0;
	if(matchCase == true)
		searchFlags |= SCFIND_MATCHCASE;
	if(matchWord == true)
		searchFlags |= SCFIND_WHOLEWORD;
	if(regex == true)
		searchFlags |= SCFIND_REGEXP | SCFIND_CXX11REGEX;
	Set<SearchFlags>(searchFlags);
	fSearchLastFlags = searchFlags;

	Set<SearchTarget>({start, end});

	fSearchLast = search;
	Sci_Position pos = SendMessage(SCI_SEARCHINTARGET, (uptr_t) search.size(), (sptr_t) search.c_str());
	if(pos != -1) {
		fSearchLastResult = Get<SearchTarget>();
		Set<Selection>(fSearchLastResult);
		return true;
	}
	return false;
}