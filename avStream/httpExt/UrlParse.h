#ifndef __URL_Parse_H__
#define __URL_Parse_H__

#ifdef _MSC_VER
#pragma warning(disable:4514)
#endif

#include <QString>

#ifdef SOCKETS_NAMESPACE
namespace SOCKETS_NAMESPACE {
#endif


/***************************************************/
/* interface of class Parse */

/** Splits a string whatever way you want.
	\ingroup util */
class UrlParse
{
public:
    UrlParse();
    UrlParse(const QString&);
    UrlParse(const QString&,const QString&);
    UrlParse(const QString&,const QString&,short);
	~UrlParse();
	short issplit(const char);
	void getsplit();
	void getsplit(QString&);
	QString getword();
	void getword(QString&);
	void getword(QString&,QString&,int);
	QString getrest();
	void getrest(QString&);
	long getvalue();
	void setbreak(const char);
	int getwordlen();
	int getrestlen();
	void enablebreak(const char c) {
		pa_enable = c;
	}
	void disablebreak(const char c) {
		pa_disable = c;
	}
	void getline();
	void getline(QString &);
	size_t getptr() { return pa_the_ptr; }
	void EnableQuote(bool b) { pa_quote = b; }

private:
	QString pa_the_str;
	QString pa_splits;
	QString pa_ord;
	size_t   pa_the_ptr;
	char  pa_breakchar;
	char  pa_enable;
	char  pa_disable;
	short pa_nospace;
	bool  pa_quote;
};

#endif // __URL_Parse_H__

