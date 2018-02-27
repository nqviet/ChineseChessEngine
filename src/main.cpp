#include <iostream>

#include "bitboard.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "tt.h"
#include "uci.h"

namespace PSQT 
{
	void init();
}

int main(int argc, char* argv[])
{
	std::cout << engine_info() << std::endl;

	UCI::init(Options);
	PSQT::init();
	Bitboards::init();
	Position::init();
	Bitbases::init();
	Search::init();
	Pawns::init();
	Threads.init();
	TT.resize(Options["Hash"]);

	UCI::loop(argc, argv);

	Threads.exit();
	return 0;
}