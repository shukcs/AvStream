#include <stdlib.h>
#include <cstring>

#include "UrlParse.h"

UrlParse::UrlParse()
:pa_the_str("")
,pa_splits("")
,pa_ord("")
,pa_the_ptr(0)
,pa_breakchar(0)
,pa_enable(0)
,pa_disable(0)
,pa_nospace(0)
,pa_quote(false)
{
}

UrlParse::UrlParse(const QString&s)
:pa_the_str(s)
,pa_splits("")
,pa_ord("")
,pa_the_ptr(0)
,pa_breakchar(0)
,pa_enable(0)
,pa_disable(0)
,pa_nospace(0)
,pa_quote(false)
{
}

UrlParse::UrlParse(const QString &s,const QString &sp)
:pa_the_str(s)
,pa_splits(sp)
,pa_ord("")
,pa_the_ptr(0)
,pa_breakchar(0)
,pa_enable(0)
,pa_disable(0)
,pa_nospace(0)
,pa_quote(false)
{
}

UrlParse::UrlParse(const QString &s,const QString &sp,short nospace)
:pa_the_str(s)
,pa_splits(sp)
,pa_ord("")
,pa_the_ptr(0)
,pa_breakchar(0)
,pa_enable(0)
,pa_disable(0)
,pa_nospace(1)
,pa_quote(false)
{
}


UrlParse::~UrlParse()
{
}

short UrlParse::issplit(const char c)
{
	for (size_t i = 0; i < pa_splits.size(); i++)
		if (pa_splits.at(i) == c)
			return 1;
	return 0;
}

#define C ((pa_the_ptr<pa_the_str.size()) ? pa_the_str.at(pa_the_ptr).cell() : 0)
void UrlParse::getsplit()
{
	size_t x;

	if (C == '=')
	{
		x = pa_the_ptr++;
	} else
	{
		while (C && (issplit(C)))
			pa_the_ptr++;
		x = pa_the_ptr;
		while (C && !issplit(C) && C != '=')
			pa_the_ptr++;
	}
	if (x == pa_the_ptr && C == '=')
		pa_the_ptr++;
	pa_ord = (x < pa_the_str.size()) ? pa_the_str.mid(x,pa_the_ptr - x) : "";
}

QString UrlParse::getword()
{
	size_t x;
	int disabled = 0;
	int quote = 0;
	int rem = 0;

	if (pa_nospace)
	{
		while (C && issplit(C))
			pa_the_ptr++;
		x = pa_the_ptr;
		while (C && !issplit(C) && (C != pa_breakchar || !pa_breakchar || disabled))
		{
			if (pa_breakchar && C == pa_disable)
				disabled = 1;
			if (pa_breakchar && C == pa_enable)
				disabled = 0;
			if (pa_quote && C == '"')
				quote = 1;
			pa_the_ptr++;
			while (quote && C && C != '"')
			{
				pa_the_ptr++;
			}
			if (pa_quote && C == '"')
			{
				pa_the_ptr++;
			}
			quote = 0;
		}
	} else
	{
		if (C == pa_breakchar && pa_breakchar)
		{
			x = pa_the_ptr++;
			rem = 1;
		} else
		{
			while (C && (C == ' ' || C == 9 || C == 13 || C == 10 || issplit(C)))
				pa_the_ptr++;
			x = pa_the_ptr;
			while (C && C != ' ' && C != 9 && C != 13 && C != 10 && !issplit(C) &&
			 (C != pa_breakchar || !pa_breakchar || disabled))
			{
				if (pa_breakchar && C == pa_disable)
					disabled = 1;
				if (pa_breakchar && C == pa_enable)
					disabled = 0;
				if (pa_quote && C == '"')
				{
					quote = 1;
				pa_the_ptr++;
				while (quote && C && C != '"')
				{
					pa_the_ptr++;
				}
				if (pa_quote && C == '"')
				{
					pa_the_ptr++;
				}
				}
				else
					pa_the_ptr++;
				quote = 0;
			}
			pa_the_ptr++;
			rem = 1;
		}
		if (x == pa_the_ptr && C == pa_breakchar && pa_breakchar)
			pa_the_ptr++;
	}
	if (x < pa_the_str.size())
	{
		pa_ord = pa_the_str.mid(x, pa_the_ptr - x - rem);
	}
	else
	{
		pa_ord = "";
	}
	return pa_ord;
}

void UrlParse::getword(QString&s)
{
	s = UrlParse::getword();
}

void UrlParse::getsplit(QString&s)
{
	UrlParse::getsplit();
	s = pa_ord;
}

void UrlParse::getword(QString&s,QString&fill,int l)
{
	UrlParse::getword();
	s = "";
	while (s.size() + pa_ord.size() < (size_t)l)
		s += fill;
	s += pa_ord;
}

QString UrlParse::getrest()
{
	QString s;
	while (C && (C == ' ' || C == 9 || issplit(C)))
		pa_the_ptr++;
	s = (pa_the_ptr < pa_the_str.size()) ? pa_the_str.mid(pa_the_ptr) : "";
	return s;
}

void UrlParse::getrest(QString&s)
{
	while (C && (C == ' ' || C == 9 || issplit(C)))
		pa_the_ptr++;
	s = (pa_the_ptr < pa_the_str.size()) ? pa_the_str.mid(pa_the_ptr) : "";
}

long UrlParse::getvalue()
{
	UrlParse::getword();
    return pa_ord.toLong();
}

void UrlParse::setbreak(const char c)
{
	pa_breakchar = c;
}

int UrlParse::getwordlen()
{
	size_t x,y = pa_the_ptr,len;

	if (C == pa_breakchar && pa_breakchar)
	{
		x = pa_the_ptr++;
	} else
	{
		while (C && (C == ' ' || C == 9 || C == 13 || C == 10 || issplit(C)))
			pa_the_ptr++;
		x = pa_the_ptr;
		while (C && C != ' ' && C != 9 && C != 13 && C != 10 && !issplit(C) && (C != pa_breakchar || !pa_breakchar))
			pa_the_ptr++;
	}
	if (x == pa_the_ptr && C == pa_breakchar && pa_breakchar)
		pa_the_ptr++;
	len = pa_the_ptr - x;
	pa_the_ptr = y;
	return (int)len;
}

int UrlParse::getrestlen()
{
	size_t y = pa_the_ptr;
	size_t len;

	while (C && (C == ' ' || C == 9 || issplit(C)))
		pa_the_ptr++;
	len = strlen(pa_the_str.toUtf8().data() + pa_the_ptr);
	pa_the_ptr = y;
	return (int)len;
}

void UrlParse::getline()
{
	size_t x;

	x = pa_the_ptr;
	while (C && C != 13 && C != 10)
		pa_the_ptr++;
	pa_ord = (x < pa_the_str.size()) ? pa_the_str.mid(x,pa_the_ptr - x) : "";
	if (C == 13)
		pa_the_ptr++;
	if (C == 10)
		pa_the_ptr++;
}

void UrlParse::getline(QString&s)
{
	getline();
	s = pa_ord;
}

/* end of implementation of class UrlParse */
/***************************************************/
#ifdef SOCKETS_NAMESPACE
}
#endif


