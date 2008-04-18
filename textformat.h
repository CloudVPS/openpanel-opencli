#ifndef _SCREEN_H
#define _SCREEN_H 1


#include <grace/value.h>
#include <grace/str.h>
#include <grace/file.h>
#include <grace/terminal.h>

// This textformatter class can print tabled on textformatter with user interaction
// default waits at every 24 lines, 
// press 'q' for end of output
// press 'space' for next 24 lines
// press 'return' for next line
//
//	prints a table like:
//	
//	Title
//	+---------+-------------------------+
//	| col1    | col2                    |
//	+---------+-------------------------+
//	| data    | data                    |
//	| data    | data                    |
//	| data    | data                    |
//	| data    | data                    |
//	+---------+-------------------------+
//


class asciitable
{
public:
					 asciitable (void) {}
					~asciitable (void) {}
					
	void			 title (const string &t) { T=t; }
	void			 header (const value &v) { H=v; }
	void			 addrow (const value &row);
	
	value			*getlines (void);
	void			 clear ();

protected:
	value			 H;
	string			 T;
	value			 rows;
	
	void			 addhline (value &, char, char, char, char);
	void			 addline (value &, const value &, bool);
	int				 maxsplitsize (const string &, char);
	int				 maxlinewidth (const string &);
};

class textformatter
{
public:
				 /// Constructor
				 textformatter (file *pfout, file *pfin, 
						 class opencliApp *app);
						 
				 /// Destructor
				~textformatter (void)
				 {
				 }

				 /// Set title on top of output
	void		 settitle 	(const string &title);
	
	
				 /// Set the output column headers, supported attr.
				 /// ("title_bold") == "true/false"
				 /// ("data_bold") 	== "true/false"
	void		 setcolumns (const value &v);
				
				 /// Add a row to our data set
	void		 addrow 	(const value &v);
	void		 adddata 	(const value &v);			
	
				 /// Clear data
	void		 clear (void);
	
				 /// output data to terminal
	void		 go (void);

	void		 outputwithpager (const string &text);
	void		 pagelines (const value &lines);

	bool 		 userconfirm (const string &what);

private:
	void		 print_title 	(void);			
	
	int 		 userinput (void);		
	string 		 _title;	///< title on top of output
	asciitable	 ascii;

	class opencliApp *papp;
	file		*_fout;		///< standard application output
	file		*_fin;		///< standard application input
};

#endif
