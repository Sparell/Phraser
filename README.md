#Phraser

"Phraser" is a phrase generator using n-grams and Markov chains to generate phrases for passphrase cracking.
The generated phraselists can be used as wordlists in your favourite password cracking application.

An academic style writeup of the project is available [here](http://www.simovits.com/sites/default/files/files/PederSparell_Linguistic_Cracking_of_Passphrases_using_Markov_Chains.pdf).

A Swedish version, which is a bit more detailed and has some additional general info about password cracking, is also available [here](http://www.simovits.com/sites/default/files/files/PederSparell-Lingvistisk_knackning_av_losenordsfraser.pdf).

The phrase generator solution is based on a markov process with two steps involved: 
  - First you create n-gram statistics (stats about how often certain character sequences occurs in a large text) with the program PhraserGram. Redirect the output to file.
  - Then use those statistics as input to the actual phrase generator which outputs phrases, one per line. The output can be redirected to file or read directly into your favourite password cracker via stdin. 

The quality of the final result is strongly dependant on the source text file (the file wich you build up the n-gram statistics from), and depending on size and quality of that file you have to experiment a bit with threshold and other params in order for Phraser to generate phrases with good quality and in a resonable time frame.
Generally, word level n-grams generate better phrases, but the source text file has to be larger.

###PhraserGram Usage
```
USAGE:
PhraserGram.exe -i PATH | -h [options]
OPTIONS:
-i PATH Path to input source text file
-n INT  Size of n-grams (-n 3 means trigrams eg. Default = 1)
-w      Generate n-grams on word level (if omitted, default is character level)
-h      Show this usage message. --help or /? or no args also works
```

###Phraser Usage
```
USAGE:
Phraser.exe -i PATH |-h [OPTIONS]
OPTIONS:
-h | --help               Show this help message
-i | --input       PATH   Path to n-gram stats file
-min               INT    Min number of chars in phrases
-max               INT    Max number of chars in phrases
-wmin | --wordsmin INT    Minimal number of words in phrases
-wmax | --wordsmax INT    Maximal number of words in phrases
-t | --threshold   INT    Ignore n-grams seen <= -t times in source text
-f | --feed        STRING Only create phrases starting with this
```

##Complete example execution:

1. `PhraserGram -i texts\SherlockBook.txt -n 3 -w > ngrams\3WSherlock.txt` (Reads a book saved in the specified path and create 3-gram stats on word level, and save the results to the file 3WSherlock.txt)
2. `Phraser -i ngrams\3WSherlock.txt -min 14 -max 16 -t 0 > phrases\L14-16T0N3WSherlock.txt` (reads that 3-gram stat file, and outputs phrases of lengths 14-16 chars, without threshold filtering, and saves the phrases to the specified file)
	
##Some links to source text file resources:
* [Lots of different files with sentences gathered from different places on internet.](http://corpora2.informatik.uni-leipzig.de/download.html)
* [Free ebooks in text format.](https://www.gutenberg.org/) (These are pretty small, but can suffice for testing etc.)
