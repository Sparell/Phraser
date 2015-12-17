using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.Text.RegularExpressions;

namespace PhraserGram
{
    class Program
    {
        static void Main(string[] args)
        {
            bool wordmode = false;
            int n = 1;
            string sourcetextfile = null;

            // Read arguments
            if(args.Length == 0) { ShowUsage(); return; }
            for (int i = 0; i < args.Length; i++)
            {
                if (args[i] == "-i")
                {
                    if (args.Length < i + 2) { Console.Error.WriteLine("\nThe -i option requires a input file path."); ShowUsage(); return; };
                    sourcetextfile = args[++i];
                    if (!File.Exists(sourcetextfile)) { Console.Error.WriteLine("\nThe file {0} could not be found. Please check path.", sourcetextfile); ShowUsage(); return; }
                }

                if (args[i] == "-n" ) if (args.Length < i + 2 || !int.TryParse(args[++i], out n)) { Console.Error.WriteLine("\nThe -n option requires an integer."); ShowUsage(); return; };
                if (args[i] == "-w") wordmode = true;
                if (args[i] == "-h" || args[i] == "--help" || args[i] == "/?") { ShowUsage(); return; } 

            }

            // Execute ngram generation if required arguments are valid
            if (string.IsNullOrWhiteSpace(sourcetextfile)) { ShowUsage(); return; }
            else
            {
                ngramextraction ngrams = new ngramextraction(sourcetextfile, n, wordmode);
            }
        }

        private static void ShowUsage()
        {
            Uri appuri = new Uri(System.Reflection.Assembly.GetExecutingAssembly().CodeBase);

            Console.WriteLine("\nUSAGE:");
            Console.WriteLine("{0} -i PATH | -h [options]", appuri.Segments.Last());
            Console.WriteLine("\nOPTIONS:");
            Console.WriteLine("-i PATH\tPath to input source text file\n");
            Console.WriteLine("-n INT\tSize of n-grams (-n 3 means trigrams eg. Default = 1)\n");
            Console.WriteLine("-w   \tGenerate n-grams on word level (if omitted, default is character level)\n");
            Console.WriteLine("-h   \tShow this usage message. --help or /? or no args also works");
        }
    }
    class ngramextraction
    {
        DateTime starttime = DateTime.Now;

        const string BREAK = ".";
        const string SPACE = "_";

        //char[] punctuation = { '.', '!', '?', ':' };
        //char[] removechars = { '\'', '\"', '-', ';', '(', ')', ',' };

        SortedDictionary<string, int> ngramlist = new SortedDictionary<string, int>();
        string inputfile;
        bool wordmode = true;

        public ngramextraction(string inputfile, int n, bool wordmode)
        {
            this.inputfile = inputfile;
            this.wordmode = wordmode;
            ngrams(n);
        }
        
        void ngrams(int n)
        {
            Console.Error.WriteLine("Started reading the source textfile at {0}", DateTime.Now.ToLongTimeString());

            Queue<string> buffer = new Queue<string>(n); // To hold a buffer of words/characters before adding to the dictionary
            if(n > 1) buffer.Enqueue(BREAK); // Marks the first word as sentence start, or first char as wordstart
            string ngram = "";

            using (var mappedFile1 = MemoryMappedFile.CreateFromFile(inputfile))
            {
                using (Stream mmStream = mappedFile1.CreateViewStream())
                {
                    using (StreamReader sr = new StreamReader(mmStream, Encoding.UTF8, true))
                    {
                        double filesize = sr.BaseStream.Length;
                        double nextprogress = filesize / 20; // Print out progress at a 5% interval

                        if (wordmode) // Wordmode (in order to gain as much performance as possible, wordmode and charmode are split here to avoid some comparisons in the while loop, though they are allmost the same)
                        {
                            while (!sr.EndOfStream)  //   && sr.BaseStream.Position < 100000    För debug (avbryter stora texter efter ett tag)
                            {
                                var line = sr.ReadLine() + " ";

                                var lineItems = GetWords(line);

                                // Add the ngram to the table (dictionary)
                                foreach (var item in lineItems)
                                {
                                    if ((item == BREAK && buffer.Count > 0 && buffer.Last() == BREAK) || string.IsNullOrWhiteSpace(item)) continue; // Skip if double BREAK or empty string (can happen if ' stands alone or with strange chars)(did not find a bullet proof regex to handle all of this)

                                    buffer.Enqueue(item);

                                    if (buffer.Count >= n) // Now we have a full ngram to add to list
                                    {
                                        ngram = buffer.Dequeue();
                                        for (int i = 0; i < n - 1; i++) ngram += " " + buffer.ElementAt(i);

                                        ngramlist[ngram] = ngramlist.ContainsKey(ngram) ? ngramlist[ngram] + 1 : 1;
                                    }
                                }
                                if (sr.BaseStream.Position > nextprogress) // Print out progress
                                {
                                    Console.Error.WriteLine("{0}% read at {1}", ((double)sr.BaseStream.Position / (double)filesize) * 100, DateTime.Now.ToLongTimeString());
                                    nextprogress = sr.BaseStream.Position + filesize / 100;
                                }
                            }
                            // Add BREAK after last row if not already there
                            if (buffer.Count > 0) // buffer count should be n-1 here
                            {
                                if (!(buffer.Last() == BREAK)) // No need to add double BREAKS in wordmode
                                {
                                    buffer.Enqueue(BREAK);
                                    ngram = buffer.Dequeue();
                                    while (buffer.Count > 0) ngram += " " + buffer.Dequeue();

                                    ngramlist[ngram] = ngramlist.ContainsKey(ngram) ? ngramlist[ngram] + 1 : 1;
                                }
                            }
                        }
                        else // Charmode (almost the same as above wordmode)
                        {
                            while (!sr.EndOfStream)  //   && sr.BaseStream.Position < 100000    För debug (avbryter stora texter efter ett tag)
                            {
                                var line = sr.ReadLine() + "\n";

                                var lineItems = GetChars(line);

                                // Add the ngrams to the table (dictionary)
                                foreach (var item in lineItems)
                                {
                                    if ((item == SPACE || item == BREAK) && buffer.Count > 0 && (buffer.Last() == BREAK || buffer.Last() == SPACE)) continue; // Skip if space after punctuation or double space (did not find a bullet proof regex to handle all of this)
                                    buffer.Enqueue(item);

                                    if (buffer.Count >= n) // Now we have a full ngram to add to list
                                    {
                                        ngram = buffer.Dequeue();
                                        for (int i = 0; i < n - 1; i++) ngram += buffer.ElementAt(i);

                                        ngramlist[ngram] = ngramlist.ContainsKey(ngram) ? ngramlist[ngram] + 1 : 1;
                                    }
                                }
                                if (sr.BaseStream.Position > nextprogress) // Print out progress
                                {
                                    Console.Error.WriteLine("{0}% read at {1}", ((double)sr.BaseStream.Position / (double)filesize) * 100, DateTime.Now.ToLongTimeString());
                                    nextprogress = sr.BaseStream.Position + filesize / 100;
                                }
                            }
                        }
                    }
                }
            }

            // If unigram (1-gram), no need for breaks or spaces... Remove
            if (n == 1) 
            {
                ngramlist.Remove(BREAK);
                ngramlist.Remove(SPACE);
            }

            Console.Error.WriteLine("File read and {0} {1}-grams generated in {2} seconds.\nOutputing...", ngramlist.Count, n, (DateTime.Now - starttime).TotalSeconds.ToString("N01"));

            // Print to stdout
            Console.OutputEncoding = UTF8Encoding.UTF8;

            foreach (KeyValuePair<string, int> pair in ngramlist)
            {
                Console.WriteLine("{0} {1}", pair.Key, pair.Value);
            }
            
            TimeSpan totalruntime = DateTime.Now - starttime;
            Console.Error.WriteLine("Done. Total run time: {0}", totalruntime); //TotalSeconds.ToString("N01"));
        }

        static private string[] GetChars(string line)
        {
            // Replace spaces with SPACE, and punctuation (sentence starts and stops) with BREAK, and then extract all alphanumeric chars (and BREAK:s)
            //MatchCollection matches = Regex.Matches(Regex.Replace(line, @"\s* |[\b.\b!\b?\b:]+|\t", BREAK), @"[\w" + BREAK + "]"); // \s* |[.!?:]
            line = Regex.Replace(line, @"\b[.!?:]+\W*", BREAK); // Mark punctuation with a BREAK and remove spaces after
            line = Regex.Replace(line, @"\s+(?!$)", SPACE); // Mark whitespaces with SPACE (exclude eof nulls in last line)
            MatchCollection matches = Regex.Matches(line, @"[A-Za-z" + BREAK + SPACE + "]");  // Replace with A-Za-zÅÄÖåäö to include swedish
            var chars = from m in matches.Cast<Match>()
                        select m.Value.ToLower(System.Globalization.CultureInfo.InvariantCulture);
            
            return chars.ToArray();
        }

        static private string[] GetWords(string line)
        {
            // Replace punctuation with BREAK to mark sentence starts and stops, and then find all words
            line = Regex.Replace(line, @"[\b.\b!\b?\b:]+\W|\t", BREAK); //@"[\b.\b!\b?\b:]+|\t", " " + BREAK + " "
            MatchCollection matches = Regex.Matches(line, @"\b[A-Za-z']+\b|[" + BREAK + "]");  // Replace with A-Za-zÅÄÖåäö to include swedish

            var words = from m in matches.Cast<Match>()
                        select m.Value.ToLower(System.Globalization.CultureInfo.InvariantCulture).Replace("\'", ""); // Remove all ':s in words eg. in we're -> were

            return words.ToArray();
        }
    }
}
