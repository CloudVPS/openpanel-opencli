#include "textformat.h"
#include "opencli.h"

// Constructor
textformatter::textformatter (file *pfout, file *pfin, 
		 		class opencliApp *app)
{
	_fout 	= pfout;
	_fin	= pfin;
	papp 	= app;
}


//	===========================================================================
///	METHOD: textformatter::settitle
//	===========================================================================
void textformatter::settitle (const string &title)
{
	_title = title;
}


//	===========================================================================
///	METHOD: textformatter::setcolumns
//	===========================================================================
void textformatter::setcolumns (const value &v)
{
	ascii.header (v);
}
			
//	===========================================================================
///	METHOD: textformatter::addrow
//	===========================================================================
void textformatter::addrow (const value &v)
{
	ascii.addrow (v);
}

//	===========================================================================
///	METHOD: textformatter::addrow
//	===========================================================================
void textformatter::adddata (const value &v)
{
	foreach (c, v)
		addrow (c);
}


//	===========================================================================
///	METHOD: textformatter::clear
//	===========================================================================
void textformatter::clear (void)
{
	ascii.clear();
	_title.crop();
}


//	===========================================================================
///	METHOD: textformatter::go
//	===========================================================================
void textformatter::go (void)
{
	pagelines (ascii.getlines());
}

void textformatter::outputwithpager (const string &data)
{
	value lines = strutil::splitlines (data);
	pagelines (lines);
}

void textformatter::pagelines (const value &lines)
{
	int todo = 22;
	for (int i=0; i<lines.count(); ++i)
	{
		papp->shell.printf ("%s\n", lines[i].cval());
		todo--;
		if (! todo)
		{
			todo = userinput ();
		}
		if (todo<0) break;
	}
}

bool textformatter::userconfirm (const string &what)
{
	string umsg = what;
	umsg.printf (" [y/N] ");
	
	papp->shell.printf (umsg);
	
	char in;
	
	papp->shell.term.termbuf.on ();
	in = papp->shell.term.termbuf.getkey ();
	papp->shell.term.termbuf.off ();
		
	if (in == 'y' || in == 'Y')
	{
		papp->shell.printf ("Yes\n");
		return true;
	}
	if (in == 'n' || in == 'N' || in == 10)
	{
		if (in != 10)
			papp->shell.printf ("No\n");

		return false;
	}
	else
	{
		papp->shell.printf ("\nUnknown input, considered as: No\n");
	}
	

	return false;
}

//	===========================================================================
///	METHOD: textformatter::userinput
//	===========================================================================
int textformatter::userinput (void)
{
	papp->shell.printf ("\033[1m<more>\033[0m");

	char in;
	papp->shell.term.termbuf.on ();
	in = papp->shell.term.termbuf.getkey ();
	papp->shell.term.termbuf.off ();
	papp->shell.printf ("\r                                           "
						"          \r");

	if (in == ' ') return 20;
	if (in == '\n') return 1;
	if (in == 'q') return -32768;
	
	return 0;
}



//	===========================================================================
///	METHOD: textformatter::print_title
//	===========================================================================
void textformatter::print_title (void)
{
	papp->shell.printf ("\033[1m%s\033[0m\n", _title.str());
}

void asciitable::clear (void)
{
	rows.clear();
}

void asciitable::addrow (const value &row)
{
	if (! rows.count())
	{
		foreach (hdr, H)
		{
			int hwidth = hdr.sval().utf8len();
			hdr("width") = hwidth;
		}
	}
	
	rows.newval() = row;
	foreach (hdr, H)
	{
		int mwidth = maxlinewidth (row[hdr.id()]);
		int cwidth = hdr("width");
		if (cwidth < mwidth) hdr("width") = mwidth;
	}
}

void asciitable::addhline (value &into, char lx, char mx, char rx, char ln)
{
	string r;
	r.strcat (lx);
	int p=0;
	foreach (hdr, H)
	{
		++p;
		string tstr;
		tstr.utf8pad (hdr("width").ival() + 2, ln);
		r.strcat (tstr);
		if (p != H.count()) r.strcat (mx);
	}
	r.strcat (rx);
	into.newval() = r;
}

void asciitable::addline (value &into, const value &fields, bool allbold)
{
	value fieldlines;
	
	foreach (f, fields)
	{
		// Skip any fields not defined in the header.
		if (! H.exists (f.id())) continue;
		value lines = strutil::split (f, '\n');
		for (int i=0; i<lines.count(); ++i)
		{
			if (((i+1) == lines.count()) &&
			    (lines[i].sval().strlen() == 0)) continue;
			    
			if (i >= fieldlines.count())
			{
				foreach (hdr, H)
				{
					fieldlines[i][hdr.id()];
				}
			}
			fieldlines[i][f.id()] = lines[i];
		}
	}
	
	foreach (line, fieldlines)
	{
		string r = "|";
		
		foreach (f, line)
		{
			value &myhdr = H[f.id()];
			bool asbold = allbold;
			
			if (! asbold) asbold = myhdr("bold");
	
			string fstr = f;
			if (fstr.utf8len() > myhdr("width").ival())
			{
				fstr.crop (fstr.utf8pos (myhdr("width").ival()));
			}
			fstr.utf8pad (myhdr("width"), ' ');

			r.strcat (' ');
			if (asbold) r.strcat ("\033[1m");
			r.strcat (fstr);
			if (asbold) r.strcat ("\033[0m");
			r.strcat (" |");
		}

		into.newval() = r;
	}
}

value *asciitable::getlines (void)
{
	returnclass (value) res retain;
	
	int totalwidth = 1;
	int maxwidth = 0;
	statstring maxwidthid;
	value remove;
	
	// Go over the header to do some width calculations.
	foreach (hdr, H)
	{
		// We got a fixed width hint from upstream.
		if (hdr("fixwidth").ival())
		{
			int nwidth = hdr("fixwidth");
			hdr("width") = nwidth;
		}
		else if (hdr("width").ival() > maxwidth)
		{
			// We exceeded the measured maximum width, this
			// will determine which column will get shrunk.
			maxwidth = hdr("width");
			maxwidthid = hdr.id();
		}
		
		// Now let's make sure there's at least _one_ record that
		// has data for this field, otherwise we will omit it.
		bool found = false;
		foreach (r, rows)
		{
			if (r[hdr.id()].sval().strlen())
			{
				found = true;
				break;
			}
		}
		
		if (! found) remove[hdr.id()];
		else
		{
			// The field is in. Sum up the width.
			totalwidth += hdr("width").ival();
			totalwidth += 3;
		}
	}
	
	foreach (removee, remove)
	{
		H.rmval (removee.id());
	}
	
	if (totalwidth > 79)
	{
		if ((maxwidth-8) > (totalwidth-79))
		{
			int nwidth = maxwidth - (totalwidth-79);
			foreach (r, rows)
			{
				r[maxwidthid] = strutil::wrap (r[maxwidthid], nwidth);
			}
			
			H[maxwidthid]("width") = nwidth;
		}
	}
	
	addhline (res, '.', '.', '.', '-');
	addline (res, H, true);
	addhline (res, '|', '+', '|', '-');
	foreach (r, rows)
	{
		addline (res, r, false);
	}
	addhline (res, '`', '^', '\'', '-');
	
	return &res;
}

int asciitable::maxsplitsize (const string &str, char spl)
{
	value elements = strutil::split (str, spl);
	int res = 0;
	foreach (l, elements)
	{
		int ln = l.sval().utf8len();
		if (ln>res) res=ln;
	}
	return res;
}

int asciitable::maxlinewidth (const string &str)
{
	return maxsplitsize (str, '\n');
}
