// Phraser.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

using namespace std;

BOOL CtrlHandler(DWORD fdwCtrlType)
{
	switch (fdwCtrlType)
	{
		// Handle the CTRL-C signal. (TODO: Not implemented yet, except for a useless beep.. Should print out some stats on progress before quitting.)
		// Pass other signals to the next handler. 
	case CTRL_C_EVENT:
		Beep(750, 300);
		return(FALSE); // When implemented and correctly handled, change this to TRUE to avoid next handler

	case CTRL_CLOSE_EVENT:
		Beep(600, 200);
		return(FALSE);

	case CTRL_BREAK_EVENT:
		Beep(900, 200);
		return FALSE;

	case CTRL_LOGOFF_EVENT:
		Beep(1000, 200);
		return FALSE;

	case CTRL_SHUTDOWN_EVENT:
		Beep(750, 500);
		return FALSE;

	default:
		return FALSE;
	}
}

class MarkovPhraseCreation {
	int n, minchars, maxchars, minwords, maxwords, threshold;
	string feed;
	map<string, multimap<int, string>> ngramset;
	multimap<int, string> startngrams;
	bool wordmode;
	int CreateCharPhrases();
	int CreateWordPhrases();
	void PrintChildCharPhrases(string, string, int);
	void PrintChildWordPhrases(string, string, int);
	void PrintChildCharPhrases(string);
	void PrintChildWordPhrases(string);
	string StateFromFeed();
public:
	MarkovPhraseCreation();
	MarkovPhraseCreation(int _n, bool _wordmode, int _minchars, int _maxchars, int _minwords, int _maxwords, int _threshold, string _feed) : n(_n), wordmode(_wordmode), minchars(_minchars), maxchars(_maxchars), minwords(_minwords), maxwords(_maxwords), threshold(_threshold), feed(_feed){};
	int CreateMap(const char *);
	int CreatePhrases();

	int phrasecount = 0;
};

MarkovPhraseCreation::MarkovPhraseCreation(){
	n = 3;
	feed = ".";
	wordmode = false;
	minchars = 12;
	maxchars = 12;
	minwords = -1;
	maxwords = -1;
	threshold = 1;
}

static void show_usage(string name)
{
	std::cerr << "\nUSAGE:\n" 
		<< name << " -i PATH |-h [OPTIONS]\n\n"
		<< "OPTIONS:\n"
		<< "-h | --help               \tShow this help message\n"
		<< "-i | --input       PATH   \tPath to n-gram stats file\n"
		<< "-min               INT    \tMin number of chars in phrases\n"
		<< "-max               INT    \tMax number of chars in phrases\n"
		<< "-wmin | --wordsmin INT    \tMinimal number of words in phrases\n"
		<< "-wmax | --wordsmax INT    \tMaximal number of words in phrases\n"
		<< "-t | --threshold   INT    \tIgnore n-grams seen <= -t times in source text\n"
		<< "-f | --feed        STRING \tOnly create phrases starting with this\n"
		<< std::endl;
}

/* Reads start of ngram input file and tries to determine if the file is a word or char n-gram file, as well as the size of n */
int GetNgramFormat(const char * ngramfile, bool& wordmode, int& n)
{
	string line;
	ifstream file;
	file.open(ngramfile);
	int success = 0;
	
	if (file.is_open())
	{
		try
		{
			int linesread = 0;
			int totalspaces = 0;
			int oldpos = 0;
			int newpos = 0;
			bool poschanged = false;
			bool ignore = false;

			// Read some rows, to figure out n and wordmode/charmode
			while (linesread <= 100 && getline(file, line))
			{
				// In wordmode, line should look like: ". a bath 22" at n=3. Hence, n = the number of spaces on one line. 
				totalspaces += count(line.begin(), line.end(), ' ');
				
				// In charmode, line should look like: ".ab 53" at n=3. Hence, n = index of first (and only) space. This index should be the same on every row.
				newpos = line.find_first_of(' ');

				linesread++;

				// Workaround because swedish/other special chars are made of two chars. Ignore position if all chars are not english. (The remaining read lines should be enough anyway)
				basic_string <char>::iterator ch;
				for (ch = line.begin(); ch != line.end(); ch++)
				{
					if (*ch > 'z' || *ch < ' ') ignore = true;
				}
				if (ignore) 
				{ 
					ignore = false;
					continue; 
				}

				// Compare first space positions with the one from former row. If changed at anytime: not char mode
				if (linesread > 1 && oldpos != newpos) poschanged = true;
				
				oldpos = newpos;
			}

			int spacesperrow = totalspaces / linesread;
			// If spacecount is more than 1 per row -> wordmode with n = spacecount/row.
			if (spacesperrow > 1)
			{
				wordmode = true;
				n = spacesperrow;
			}
			// If spacecount is 1 per row and space position not changed -> Charmode with n = space position
			else if (spacesperrow == 1 && !poschanged)
			{
				wordmode = false;
				n = newpos;
			}
			// If spacecount is 1 per row and space position is changed -> wordmode with n = 1
			else if (spacesperrow == 1 && poschanged)
			{
				wordmode = true;
				n = 1;
			}
			else throw exception("Char/word mode and n couldn't be interpreted from file. Check format in n-gram text file.");

			clog << "File interpreted as a " << (wordmode ? "word " : "char ") << n << "-gram file.\n";
		}
		catch (const exception& e) 
		{
			cerr << "Error: Unexpected format of text file " << ngramfile << "\n" << e.what() << endl;
			success = 1;
		}
	}
	else
	{
		cerr << "Unable to open file " << ngramfile;
		return 1;
	}

	file.close();
	return success;
}

int MarkovPhraseCreation::CreateMap(const char * ngramfile){
	string line;
	string restofline;
	ifstream textfile;
	textfile.open(ngramfile);
	time_t starttime = time(NULL);

	if (textfile.is_open())
	{
		clog << "Reading n-gram file...\n";

		int pos = 0;

		string startstate = StateFromFeed();
		int startstatelength = startstate.length();

		while (getline(textfile, line))
		{
			if (count(line.begin(), line.end(), BREAK[0]) > 1) continue; // If two BREAKs on one row, the phrase is useless/too short (?)

			restofline = line;
			string splitstring[2];

			pos = restofline.find_last_of(" ");
			splitstring[1] = restofline.substr(pos + 1); // Extract the occurance number
			int occurance = atoi(splitstring[1].c_str());
			if (occurance <= threshold) continue;

			restofline.erase(pos); // Erase the occurance number

			if (restofline.substr(0, startstatelength) == startstate) startngrams.insert({ occurance, restofline }); // Add startfeed // TODO: Gör om denna så att den på något sätt inkluderar fler states. T.ex med feed 'iamthe' bör state 'm_the' inkluderas(?).

			if (wordmode)
			{
				pos = restofline.find_last_of(" "); // Find the position of the last word
				splitstring[0] = restofline.substr(pos + 1); // Extract the last unit (word)
				if (n == 1) pos++; // If n==1 last_of " " is not found, hence pos=-1. Compensate for that to avoid exception later on
			}
			else splitstring[0] = restofline.substr(--pos); // Extract the last unit (char)

			restofline.erase(pos); // And the rest is the "current state"

			ngramset[restofline].insert({ occurance, splitstring[0] });

		}
		textfile.close();

		clog << "All " << n << "-grams loaded in " << time(NULL) - starttime << " seconds.\n";

		return 0;
	}

	else
	{
		cerr << "Unable to open file " << ngramfile;
		return 1;
	}
}

string MarkovPhraseCreation::StateFromFeed()
{
	if (n == 1) return ""; // In case of unigram, state is not interesting, and should be empty

	string state = "."; 
	if (feed.length() >= n - 1) state = feed.substr(feed.length() - (n - 1)); // The state is given completely by feed
	else state.append(feed); // If state can not be derived from feed (feed is shorter than state), start the state with a BREAK (sentence start) and append whatever feed we've got
		
	return state;
}

bool startswithdot(const pair<string, multimap<int, string>>& node) //TEST (unused)
{
	return node.first[0] == '.';
}

int MarkovPhraseCreation::CreatePhrases()
{
	clog << "Starting generating phrases..." << endl;
	if (wordmode) return CreateWordPhrases();
	else return CreateCharPhrases();
}

int MarkovPhraseCreation::CreateCharPhrases(){
	try
	{
		if (n == 1)
			PrintChildCharPhrases(feed); // Special case n = 1. Unigram phrases has no state awareness.
		else
		{
			// Iterate through the most common starts

			__int64 progress = 0;

			multimap<int, string>::reverse_iterator nextstartstate;
			for (nextstartstate = startngrams.rbegin(); nextstartstate != startngrams.rend(); ++nextstartstate)
			{
				string startfeed = (feed.length() >= n) ? feed : nextstartstate->second;
				int feedwords = count(startfeed.begin(), startfeed.end(), SPACE[0]);
				startfeed.erase(remove(startfeed.begin(), startfeed.end(), BREAK[0]), startfeed.end());
				startfeed.erase(remove(startfeed.begin(), startfeed.end(), SPACE[0]), startfeed.end());

				PrintChildCharPhrases(startfeed, nextstartstate->second.substr(nextstartstate->second.length() - (n - 1)), 1+feedwords);
				
				progress++;
				time_t progresstime = time(NULL);
				clog << "Done with phrases starting with \"" << startfeed << "\". About " << progress*100/startngrams.size() << "% done at " <<  ctime(&progresstime);
			}
		}
	}
	catch (const exception& e) {
		cerr << e.what() << endl;
		return 1;
	}
	return 0;
}

int MarkovPhraseCreation::CreateWordPhrases()
{
	try
	{
		if (n == 1)
			PrintChildWordPhrases(feed); // Special case n = 1. Unigram phrases has no state awareness.
		else
		{
			// Iterate through the most common starts
			__int64 progress = 0;

			multimap<int, string>::reverse_iterator nextstartstate;
			for (nextstartstate = startngrams.rbegin(); nextstartstate != startngrams.rend(); ++nextstartstate)
			{
				string startphrase = feed;
				startphrase.append(nextstartstate->second);
				string startstate = nextstartstate->second;

				// Prepare the start phrase by removing BREAKs and whitespaces
				startphrase.erase(remove(startphrase.begin(), startphrase.end(), BREAK[0]), startphrase.end());
				startphrase.erase(remove(startphrase.begin(), startphrase.end(), ' '), startphrase.end());

				// And the start state by using the n-1 last words
				int statebreakpos = nextstartstate->second.size() - 1;
				for (int i = 0; i < n - 1; i++)
					statebreakpos = nextstartstate->second.substr(0, statebreakpos).find_last_of(' ');

				startstate = nextstartstate->second.substr(statebreakpos + 1);

				// Start the recursive chain to print all phrases based on this start
				PrintChildWordPhrases(startphrase, startstate, n-1);

				progress++;
				time_t progresstime = time(NULL);
				clog << "Done with phrases starting with \"" << startphrase << "\". About " << progress * 100 / startngrams.size() << "% done at " << ctime(&progresstime);

			}
		}
	}
	catch (const exception& e) {
		cerr << e.what() << endl;
		return 1;
	}
	return 0;
}

	// Given a phrase and state, iterate through children of that state (in descending order on occurance) to set next state
	// Recursively repeat until goal is reached (max chars/number of words)
void MarkovPhraseCreation::PrintChildCharPhrases(string phrase, string state, int wordcount){
	int phrasesize = phrase.size();

	if (phrasesize > maxchars || wordcount > maxwords) return;
	if (phrasesize < maxchars)
	{
		multimap<int, string>::reverse_iterator nextChar;
		for (nextChar = ngramset[state].rbegin(); nextChar != ngramset[state].rend(); ++nextChar)
		{
			if (nextChar->second == BREAK) continue; // Skip if BREAK is encountered before target length etc is reached (TODO: perhaps print to cout if minchars is reached?)
			if (nextChar->second != SPACE)
			{
				string newphrase = phrase.substr().append(nextChar->second);
				PrintChildCharPhrases(newphrase, state.substr(1).append(nextChar->second), wordcount);
			}
			else 
				PrintChildCharPhrases(phrase, state.substr(1).append(nextChar->second), wordcount + 1);
		}
	}
	if (phrasesize >= minchars || phrasesize == maxchars) // TODO: Perhaps move this up a few lines to the BREAK check above?
	{
		if (minwords == -1 || wordcount >= minwords)
		{
			// Check if phrase can end with a break (if the state has a BREAK in its posible next states list)
			multimap<int, string>::reverse_iterator nextChar;
			for (nextChar = ngramset[state].rbegin(); nextChar != ngramset[state].rend(); ++nextChar)
			{
				if (nextChar->second == BREAK)
				{
					// Output
					cout << phrase << endl;
					// Count
					phrasecount++;
				}
			}
		}
	}
}

/* Special case when n = 1. Since unigrams do not have state awareness, and are treated special, they are separated to this function to avoid a lot of checks for n=1, hence creating more efficiancy. */
void MarkovPhraseCreation::PrintChildCharPhrases(string phrase){
	int phrasesize = phrase.size();

	if (phrasesize > maxchars) return;
	if (phrasesize < maxchars)
	{
		multimap<int, string>::reverse_iterator nextChar;
		for (nextChar = ngramset[""].rbegin(); nextChar != ngramset[""].rend(); ++nextChar)
		{
			string newphrase = phrase.substr().append(nextChar->second);
			PrintChildCharPhrases(newphrase);
		}
	}
	if (phrasesize >= minchars || phrasesize == maxchars)
	{
		// Output
		cout << phrase << endl;
		// Count
		phrasecount++;
	}
}

/* Special case when n = 1. Since unigrams do not have state awareness, and are treated special, they are separated to this function to avoid a lot of checks for n=1, hence creating more efficiancy. */
void MarkovPhraseCreation::PrintChildWordPhrases(string phrase){
	int phrasesize = phrase.size();

	if (phrasesize > maxchars) return;
	if (phrasesize < maxchars)
	{
		multimap<int, string>::reverse_iterator nextWord;
		for (nextWord = ngramset[""].rbegin(); nextWord != ngramset[""].rend(); ++nextWord)
		{
			string newphrase = phrase.substr().append(nextWord->second);
			PrintChildWordPhrases(newphrase);
		}
	}
	if (phrasesize >= minchars || phrasesize == maxchars)
	{
		// Output
		cout << phrase << endl;
		// Count
		phrasecount++;
	}
}


// Given a phrase and state, iterate through children of that state (in descending order on occurance) to set next state
// Recursively repeat until goal is reached (max chars/number of words)
void MarkovPhraseCreation::PrintChildWordPhrases(string phrase, string state, int wordcount){
	int phrasesize = phrase.size();

	if (phrasesize > maxchars || wordcount > maxwords) return;
	if (phrasesize < maxchars)
	{
		multimap<int, string>::reverse_iterator nextWord;
		for (nextWord = ngramset[state].rbegin(); nextWord != ngramset[state].rend(); ++nextWord)
		{
			if (nextWord->second != BREAK)
			{
				string newphrase = phrase.substr().append(nextWord->second);
				PrintChildWordPhrases(newphrase, state.substr(state.find_first_of(' ') + 1).append(" ").append(nextWord->second), wordcount + 1);
			}
		}
	}
	if (phrasesize >= minchars || phrasesize == maxchars)
	{
		if (minwords == -1 || wordcount >= minwords)
		{
			// Check if phrase can end with a break (i.e. if the state has a BREAK in its list of posible next states)
			multimap<int, string>::reverse_iterator nextWord;
			for (nextWord = ngramset[state].rbegin(); nextWord != ngramset[state].rend(); ++nextWord)
			{
				if (nextWord->second == BREAK)
				{
					// Output
					cout << phrase << endl;
					// Count
					phrasecount++;
				}
			}
		}
	}
}


// Not used, n-gram extraction implemented in C# instead
/*int unigrams(const char * sourceTextFile)
{
	FILE * textfile;
	fopen_s(&textfile, sourceTextFile, "r");

		map<string, int> M;
		map<string, int>::iterator j;
		char t[100];
		
		while (fscanf_s(textfile, "%s", t, _countof(t)) != EOF) // Simple word scan based on only whitespaces.
			M[string(t)]++;
		
		for (j = M.begin(); j != M.end(); ++j)
			cout << j->first << " " << j->second << "\n";
		
		fclose(textfile);
		return 0;

}*/

int main(int argc, char* argv[])
{
	// Handle arguments
	if (argc < 3) {
		show_usage(argv[0]);
		return 1;
	}
	
	string inputfile, feedarg;
	int n = 3;
	int minchars = 12;
	int maxchars = 12;
	bool wordmode = false;
	int minwords = -1;
	int maxwords = -1;
	int threshold = 1;

	for (int i = 1; i < argc; ++i) {
		string arg = argv[i];
		if ((arg == "-h") || (arg == "--help")) {
			show_usage(argv[0]);
			return 0;
		}
		else if ((arg == "-i") || (arg == "--input")) {
			if (i + 1 < argc) { // Make sure we aren't at the end of argv!
				inputfile = argv[++i]; // Increment 'i' so we don't get the argument as the next argv[i].
			}
			else {
				std::cerr << "Input option requires a path argument." << std::endl;
				show_usage(argv[0]);
				return 1;
			}
		}
		else if (arg == "-min") {
			if (i + 1 < argc) { // Make sure we aren't at the end of argv!
				minchars = atoi(argv[++i]);
			}
			else {
				std::cerr << "Min option requires an int argument." << std::endl;
				show_usage(argv[0]);
				return 1;
			}
		}
		else if (arg == "-max") {
			if (i + 1 < argc) { // Make sure we aren't at the end of argv!
				maxchars = atoi(argv[++i]);
			}
			else {
				std::cerr << "Max option requires an int argument." << std::endl;
				show_usage(argv[0]);
				return 1;
			}
		}
		else if ((arg == "-wmin") || (arg == "--wordsmin")) {
			if (i + 1 < argc) { // Make sure we aren't at the end of argv!
				minwords = atoi(argv[++i]);
			}
			else {
				std::cerr << "Min words option requires an int argument." << std::endl;
				show_usage(argv[0]);
				return 1;
			}
		}
		else if ((arg == "-wmax") || (arg == "--wordsmax")) {
			if (i + 1 < argc) { // Make sure we aren't at the end of argv!
				maxwords = atoi(argv[++i]);
			}
			else {
				std::cerr << "Max words option requires an int argument." << std::endl;
				show_usage(argv[0]);
				return 1;
			}
		}
		else if ((arg == "-t") || (arg == "--threshold")) {
			if (i + 1 < argc) { // Make sure we aren't at the end of argv!
				threshold = atoi(argv[++i]);
			}
			else {
				std::cerr << "Threshold option requires an int argument." << std::endl;
				show_usage(argv[0]);
				return 1;
			}
		}
		else if ((arg == "-f") || (arg == "--feed")) {
			if (i + 1 < argc) { // Make sure we aren't at the end of argv!
				feedarg = argv[++i];
			}
			else {
				std::cerr << "Feed option requires a string argument." << std::endl;
				show_usage(argv[0]);
				return 1;
			}
		}
	}
	if (minchars > maxchars)
	{ 
		minchars = maxchars;
		cerr << "Option -min can't be higher than -max (currently "<< maxchars << "), using -min " << maxchars << endl;
	}
	if (minwords > maxwords)
	{
		minwords = maxwords;
		cerr << "Option -wmin can't be higher than -wmax (currently " << maxwords << "), using -wmin " << maxwords << endl;
	}
	if (inputfile.empty())
	{
		cerr << "Couldn't read file " << inputfile << endl;
		return 1;
	}

	time_t starttime = time(NULL); // A timestamp at start of execution for benchmarking/status

	// Read out n from file row format	
	if (GetNgramFormat(inputfile.c_str(), wordmode, n) == 1) return 1;

	// If not word count target is set, set it to maxchars. (There can't be more words than chars)
	if (maxwords == -1) maxwords = maxchars;

	// Init the phrase creation process
	MarkovPhraseCreation markovprocess(n, wordmode, minchars, maxchars, minwords, maxwords, threshold, feedarg);

	// And fill its map with ngram stats from the ngram file
	if (markovprocess.CreateMap(inputfile.c_str()) == 1) return 1;

	// Set a controlhandler to handle ctrl-c commands and be able to print out stats at forced quit		TEST!!!!
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);

	// Create the phrases from ngrams
	markovprocess.CreatePhrases();
	
	// Print out run stats
	double runtime = difftime(time(NULL), starttime);
 	clog << "\nTotal run time (s): " << runtime << endl;
	clog << markovprocess.phrasecount << " phrases generated at the speed of " << ((runtime == 0) ? markovprocess.phrasecount : markovprocess.phrasecount/runtime) << " phrases/second.";

	return 0;
 }

