#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "misc.h"
#include "thread.h"

using namespace std;

namespace
{

/// Version number. If Version is left empty, then compile date in the format
/// DD-MM-YY and show in engine_info.
const string Version = "8";

/// Our fancy logging facility. The trick here is to replace cin.rdbuf() and
/// cout.rdbuf() with two Tie objects that tie cin and cout to a file stream. We
/// can toggle the logging of std::cout and std:cin at runtime whilst preserving
/// usual I/O functionality, all without changing a single line of code!
/// Idea from http://groups.google.com/group/comp.lang.c++/msg/1d941c0f26ea0d81

struct Tie : public streambuf 
{ 
	// MSVC requires split streambuf for cin and cout

	Tie(streambuf* b, streambuf* l) : buf(b), logBuf(l) {}

	int sync() { return logBuf->pubsync(), buf->pubsync(); }
	int overflow(int c) { return log(buf->sputc((char)c), "<< "); }
	int underflow() { return buf->sgetc(); }
	int uflow() { return log(buf->sbumpc(), ">> "); }

	streambuf *buf, *logBuf;

	int log(int c, const char* prefix) {

		static int last = '\n'; // Single log file

		if (last == '\n')
			logBuf->sputn(prefix, 3);

		return last = logBuf->sputc((char)c);
	}
};

class Logger 
{

	Logger() : in(cin.rdbuf(), file.rdbuf()), out(cout.rdbuf(), file.rdbuf()) {}
	~Logger() { start(""); }

	ofstream file;
	Tie in, out;

public:
	static void start(const std::string& fname) {

		static Logger l;

		if (!fname.empty() && !l.file.is_open())
		{
			l.file.open(fname, ifstream::out);
			cin.rdbuf(&l.in);
			cout.rdbuf(&l.out);
		}
		else if (fname.empty() && l.file.is_open())
		{
			cout.rdbuf(l.out.buf);
			cin.rdbuf(l.in.buf);
			l.file.close();
		}
	}
};

} // namespace

/// engine_info() returns the full name of the current Stockfish version. This
/// will be either "Stockfish <Tag> DD-MM-YY" (where DD-MM-YY is the date when
/// the program was compiled) or "Stockfish <Version>", depending on whether
/// Version is empty.

const string engine_info(bool to_uci) {

	const string months("Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec");
	string month, day, year;
	stringstream ss, date(__DATE__); // From compiler, format is "Sep 21 2008"

	ss << "Stockfish " << Version << setfill('0');

	if (Version.empty())
	{
		date >> month >> day >> year;
		ss << setw(2) << day << setw(2) << (1 + months.find(month) / 4) << year.substr(2);
	}

	ss  << " x64"
		<< " BMI2"
		<< (to_uci ? "\nid author " : " by ")
		<< "T. Romstad, M. Costalba, J. Kiiski, G. Linscott";

	return ss.str();
}

/// Used to serialize access to std::cout to avoid multiple threads writing at
/// the same time.

std::ostream& operator<<(std::ostream& os, SyncCout sc) {

	static Mutex m;

	if (sc == IO_LOCK)
		m.lock();

	if (sc == IO_UNLOCK)
		m.unlock();

	return os;
}

/// Trampoline helper to avoid moving Logger to misc.h
void start_logger(const std::string& fname) 
{ 
	Logger::start(fname); 
}

void prefetch(void* addr)
{
	_mm_prefetch((char*)addr, _MM_HINT_T0);
}