#include "opencli.h"
#include "sessionproxy.h"
#include "version.h"
#include <grace/http.h>
#include <grace/xmlschema.h>
#include <grace/system.h>
#include <grace/file.h>
#include <grace/filesystem.h>

$exception (obsoletedClassStateException, "Interface in outmoded 'class' state");

bool SHOULDEXIT = false;
bool INTERACTIVE = true;
APPOBJECT(opencliApp);

void cbkeepalive (void *obj)
{
	static time_t last = 0;
	time_t now = time (NULL);
	if (! last) last = now;
	
	if ((now-last) > 15)
	{
		opencliApp *app = (opencliApp *) obj;
		app->keepalive ();
		last = now;
	}
}

// ==========================================================================
// METHOD opencliApp::main
// ==========================================================================
int opencliApp::main (void)
{
	if (argv.exists ("--host"))
	{
		conn.seturl ("https://%s:4089/json" %format (argv["--host"]));
	}

	// Since actual opencore clients are always in some form privileged
	// towards its socket, there is an assumption of trust between the
	// clients and the server. Although authentication of the session is
	// still necessary, trusting the clients not to lie about the true
	// origins of a session is a reasonable compromise. For the cli this
	// means tracking the connection back to the ssh connection that
	// spawned it.
	if (env.exists ("SSH_CLIENT"))
	{
		string cl;
		cl = env["SSH_CLIENT"];
		
		// In case of ipv6-notation, reduce back to the ipv4 address.
		if (cl.strncmp ("::ffff:", 7) == 0)
		{
			cl = cl.mid (7);
		}
		delete cl.cutafter (" ");
		conn.setsource (cl);
	}
	
	// Execute the commandline.
	commandline ();
	
	// Ok we're done, drop the session.
	conn.dropsession ();
	return 0;
}

#define PRINTERR(x) ferr.printf (x ": %s\n", conn.geterror().str())
				
// ==========================================================================
// METHOD opencliApp::commandline
// ==========================================================================
void opencliApp::commandline (void)
{
	// Storage for login credentials.
	string user;
	string pass;

	// Get the username and password from the user.
	if (argv.exists ("--login") || argv.exists ("--host"))
	{
		fout.printf ("User Access Authentication\n");
		user = shell.term.readline ("Username: ", true);
		if (user.strlen())
		{
			pass = shell.term.readpass ("Password: ");
		}
		else if (argv.exists ("--host"))
		{
			ferr.writeln ("% Can't log in with no username");
			return;
		}
	}
	
	shell.term.termbuf.on ();
	
	shell.setctrlmacro ('d', "exit", true);
	shell.setctrlmacro ('z', "end", true);
	
	// Set up the various data sources for the command line syntax.
	shell.addsrc ("@classobject", &opencliApp::srcClassObject);
	shell.addsrc ("@class", &opencliApp::srcClass);
	shell.addsrc ("@userclassparam", &opencliApp::srcUserOrClassOrParam);
	shell.addsrc ("@classes", &opencliApp::srcClasses);
	shell.addsrc ("@field", &opencliApp::srcField);
	shell.addsrc ("@id", &opencliApp::srcId);
	shell.addsrc ("@idparam", &opencliApp::srcIdParam);
	shell.addsrc ("@param", &opencliApp::srcParam);
	shell.addsrc ("@user", &opencliApp::srcUser);
	shell.addsrc ("@methods", &opencliApp::srcMethods);
	shell.addsrc ("@methodparams", &opencliApp::srcMethodparams);
	
	// Set up the syntax definitions
	shell.addsyntax ("configure @classobject", &opencliApp::cmdSelect);	  
	shell.addsyntax ("configure @classobject @classobject", &opencliApp::cmdSelect);
	shell.addsyntax ("show", &opencliApp::cmdShow);
	shell.addsyntax ("show @classobject", &opencliApp::cmdList);
	shell.addsyntax ("query @classes @field", &opencliApp::cmdQuery);				 
	shell.addsyntax ("query @classes @field #", &opencliApp::cmdQuery);
	shell.addsyntax ("create @userclassparam", &opencliApp::cmdCreate);
	shell.addsyntax ("create @userclassparam #", &opencliApp::cmdCreate);
	shell.addsyntax ("set @param", &opencliApp::cmdSet);
	shell.addsyntax ("set @param #", &opencliApp::cmdSet);
	shell.addsyntax ("update @class @idparam", 	&opencliApp::cmdUpdate);
	shell.addsyntax ("update @class @idparam @param", &opencliApp::cmdUpdate);
	shell.addsyntax ("update @class @idparam @param #", &opencliApp::cmdUpdate);
	shell.addsyntax ("delete @class", &opencliApp::cmdDelete);
	shell.addsyntax ("delete @class @id", 	&opencliApp::cmdDelete);
	shell.addsyntax ("exec @class @id @methods", &opencliApp::cmdMethod);
	shell.addsyntax ("exec @class @id @methods @methodparams", &opencliApp::cmdMethod);
	shell.addsyntax ("exec @class @id @methods @methodparams #", &opencliApp::cmdMethod);
	shell.addsyntax ("debug session", &opencliApp::cmdDebug);
	shell.addsyntax ("debug capabilities @class", &opencliApp::cmdDebug);
	shell.addsyntax ("debug params @class", &opencliApp::cmdDebug);
	shell.addsyntax ("debug classmapping", &opencliApp::cmdDebug);
	shell.addsyntax ("debug ignore_capb", &opencliApp::cmdDebug);		
	shell.addsyntax ("debug restore_capb", &opencliApp::cmdDebug);	
	shell.addsyntax ("debug rawrequest", &opencliApp::cmdDebug);		
	shell.addsyntax ("version", &opencliApp::cmdVersion);		
	shell.addsyntax ("exit", &opencliApp::cmdUpcontext);
	shell.addsyntax ("..", &opencliApp::cmdUpcontext);
	shell.addsyntax ("end", &opencliApp::cmdUpcontext);
	shell.addsyntax ("quit", &opencliApp::cmdExit);
	shell.addsyntax ("help", &opencliApp::cmdHelp);
	
	// Fill in help information for the above syntax.
	shell.addhelp ("configure", "Enter configuration for a specific object");
	shell.addhelp ("show", 		"Show current object or child objects");
	
	shell.addhelp ("create", 	"Create a new object");
	shell.addhelp ("set",		"Change field(s) of current object");
	shell.addhelp ("update", 	"Update an existing object");
	shell.addhelp ("delete", 	"Delete an existing object");
	shell.addhelp ("query",		"Query data with preselected columns");
	shell.addhelp ("exec", 		"Execute a remote class method");
	shell.addhelp ("help",      "Brief help message");
	
	shell.addhelp ("debug", 			"Debug");
	shell.addhelp ("debug session", 	"Output internal session");
	shell.addhelp ("debug capabilities","Output class capabilities");
	shell.addhelp ("debug params", 		"Debug command parameter input");
	shell.addhelp ("debug classmapping","Debug internal class mapping");
	shell.addhelp ("debug ignore_capb", "Ignore capabilities");
	shell.addhelp ("debug restore_capb","Restore capabilities");
	shell.addhelp ("debug rawrequest",	"Send a request using vi");
	
	shell.addhelp ("version",	"Show current version");
	shell.addhelp ("end", 		"Go back to top level context");
	shell.addhelp ("exit", 		"Go up to parent object");
	shell.addhelp ("..",		"Go up to parent object");
	shell.addhelp ("quit",		"Exit system");

	shell.term.termbuf.loadhistory ("home:.openclihistory");
	shell.term.termbuf.setidlecb (cbkeepalive, this);

	// Read login credentials from terminal.
	try
	{
		// Try to get a session with these credentials.
		if (! conn.getsession (user, pass))
		{
			ferr.writeln ("%% Session error: %s" %format (conn.geterror()));
			shell.term.off ();
			exit (1);
		}

		// Setup shortname translation table
		coreclass cclass (conn, "ROOT");
		foreach (vd, cclass.children ())
		{
			shortnames[vd["shortname"]] = vd["description"];
			shortnames[vd["shortname"]]("realid") = vd["id"];
		}
		
		// Create path entry
		ctx.root ();
	
		if (argv["*"].count() || argv.exists ("--shellcmd"))
		{
			INTERACTIVE = false;
			value cmdlist;

			if (argv.exists("--shellcmd"))
			{
				cmdlist = strutil::splitquoted (argv["--shellcmd"]);
			}
			else
			{
				cmdlist = argv["*"];
			}

			foreach (cmd, cmdlist)
			{
				fout.writeln ("-> %s" %format (cmd));
				shell.singlecmd (cmd);
			}
                	shell.term.off ();
		}
		else
		{
			// Create terminal prompt
			prom = "[opencli]% ";
			shell.run (prom);
		}
	}
	catch (exception e)
	{
		ferr.writeln ("\n%% Exception: %s." %format (e.description));
		shell.term.off ();
	}
	
	shell.term.termbuf.savehistory ("home:.openclihistory");
	fs.chmod ("home:.openclihistory", 0600);
}

// ==========================================================================
// METHOD opencliApp::keepalive
// ==========================================================================
void opencliApp::keepalive (void)
{
	conn.keepalive ();
}

// ==========================================================================
// METHOD opencliApp::cmdversion
// ==========================================================================
int	 opencliApp::cmdVersion (const value &argv)
{
	fout.writeln ("OpenCLI %s" %format (version::release));
	fout.writeln ("%s %s@%s" %format (version::date, version::user,
									  version::hostname));
	return 0;
}

// ==========================================================================
// METHOD opencliApp::cmdDebug
// ==========================================================================
int	 opencliApp::cmdDebug (const value &argv)
{
	string txt;
	string ufname, lcmd;
	lcmd = "less %s" %format (ufname);

	caseselector (argv[1])
	{
		incaseof ("class") :
			break;
			
		incaseof ("session") :
			value 	tmp = conn.getCurrentClass ();
			txt = tmp.toxml ();
			formatter.outputwithpager (txt);
			break;
			
		incaseof ("capabilities") :
			value cap;
			cap = conn.classcapabilities (shortnames[argv[2]]("realid"));
			txt = cap.toxml ();
			formatter.outputwithpager (txt);
			break;

		incaseof ("params") :
			value prm;
			prm = conn.getparams (shortnames[argv[2]]("realid"),
							argv[3].sval());
			txt = prm.toxml ();
			formatter.outputwithpager (txt);
			break;
			
		incaseof ("classmapping") :
			txt = shortnames.toxml ();
			formatter.outputwithpager (txt);
			break;
			
			
		incaseof ("rawrequest") :
			// Open the request in vi
			string vicmd = "vi %s" %format (ufname);
			if ( 0 != kernel.sh (vicmd))
			{
				ferr.printf ("Err: Could not start vi\n");
				break;
			}
			
			// do request
			value result, request;
			request.loadxml (ufname);
			result = conn.sendrawrequest (request);
			result.savexml (ufname);
			
			// Display result in less
			kernel.sh (lcmd);
			fs.rm (ufname);

			break;
			
		incaseof ("ignore_capb") :
			debug_or = true;
			break;
		incaseof ("restore_capb") :
			debug_or = false;
			break;
			
		defaultcase :
			ferr.printf ("%% Unknown command\n");
			break;
	}

	return 0;
}

// ==========================================================================
// METHOD opencliApp::cmdMethod
// ==========================================================================
int opencliApp::cmdMethod (const value &argv)
{
	string 	parentid;		// Parent id in current context
	string 	id;				// Current object id or empty
	string	mclass;		// Current class ID
	value	params;			// Build list wit parameters

	parentid = ctx.parentuuid ();
	
	mclass 	 = shortnames[argv[1]]("realid");	// class id
	id	= argv[2];

	// extract parameters
	for (int i=4; i<argv.count(); i++)
	{
		string pval, pname;
		pval  = argv[i].sval();
		pname = pval.cutat('=');
		params[pname] = pval;
	}

	if (conn.execmethod (mclass, argv[3].sval(), params, parentid, id))
	{
		fout.writeln ("% Error executing method call");
	}
	else
	{
		fout.writeln ("% Request executed");
	}

	return 0;
}

int opencliApp::cmdHelp (const value &argv)
{
	fout.writeln (
		"Welcome to the opencli shell.\n"
		"Use the question mark key '?' to explore your command line.\n"
		"For further information, see the opencli manpage."
	);
	return 0;
}

// ==========================================================================
// METHOD opencliApp::cmdQuery
// ==========================================================================
int opencliApp::cmdQuery (const value &argv)
{
	string title;
	value  columns, vdata, records;
	
	// Create columns
	for (int i=2; i<argv.count(); i++)
	{
		columns[argv[i]] = argv[i].sval();
		columns[argv[i]]("bold") = false;
	}

	// Get all records from class
	records 	= conn.getrecords (argv[1], "");
	
	foreach (record, records)
	{
		vdata.newval();
		for (int i=2; i<argv.count(); i++)
		{	
			vdata[-1][argv[i].id()] = record[argv[i]];
		}
	}
	
	title = "Query result for class '%s': %i record(s)"
				%format (argv[1], records.count());
				  
	// configure output
	formatter.clear ();
	formatter.settitle (title);
	formatter.setcolumns (columns);
	formatter.adddata (vdata);
	
	// Print table
	formatter.go ();

	return 0;
}

string *opencliApp::uuidToUser (const statstring &uuid)
{
	returnclass (string) res retain;
	static value cache;
	
	if (cache.exists (uuid))
	{
		res = cache[uuid];
		return &res;
	}
	
	value rec;
	rec = conn.getrecords ("User","");
	foreach (u, rec)
	{
		cache[u["uuid"]] = u.id();
	}
	
	if (cache.exists (uuid)) res = cache[uuid];
	return &res;
}

// ==========================================================================
// METHOD opencliApp::cmdShow
// ==========================================================================
int opencliApp::cmdShow (const value &argv)
{
	if (ctx.atRoot())
	{
		fout.writeln ("% No local data at this level");
		return 0;
	}

	string title;
	value vdata;
	value columns = $("field", $attr("bold",true)->$val("Field")) ->
					$("value", $attr("bold",false)->$val("Value")) ->
					$("desc", $attr("bold",false)->$val("Description"));

	statstring classid = ctx.currentClass ();
	coreclass cclass (conn, classid);
	
	value params = cclass.parameters ();

	string uname = uuidToUser (crecord["ownerid"]);
	if (! uname) uname = "root";

	vdata.newval() =
		$("field", "owner") ->
		$("value", uname) ->
		$("desc", "Object owner");
	
	foreach (v, params)
	{
		if (v["visible"])
		{
			vdata.newval() =
				$("field",v.id()) ->
				$("value",crecord[v.id()].sval()) ->
				$("desc",v["description"]);
		}
	}
	
	if (! vdata.count())
	{
		fout.writeln ("% \033[1mNo properties\033[0m");
	}

	// configure output
	formatter.clear ();
	formatter.settitle (title);
	formatter.setcolumns (columns);
	formatter.adddata (vdata);
	formatter.go ();

	return 0;
}

// ==========================================================================
// METHOD opencliApp::cmdList
// ==========================================================================
int opencliApp::cmdList (const value &argv)
{
	if (argv.count() == 1)
	{
		formatter.clear ();
		formatter.setcolumns ($("id", $attr("bold",true)->$val("Class ID")) ->
						   $("desc", $attr("bold",false)->$val("Description")));

		value coldata;						   
		foreach (v, shortnames)
		{
			coldata.newval() = $("id", v.id()) -> $("desc", v.sval());
		}

		formatter.adddata (coldata);
		formatter.go ();
	}
	else
	{
		statstring classid = shortnames[argv[1]]("realid");
		statstring tpid = ctx.uuid ();
		value records = conn.getrecords (classid, tpid);
		coreclass cclass (conn, classid);
		
		if (cclass.autoindex ())
		{
			value nrec;
			
			UUID.processlist (classid, tpid, records);
			
			foreach (trec, records)
			{
				statstring rid;
				rid = UUID.tempfromuuid (trec.id(), classid, tpid);
				nrec[rid] = trec;
				nrec[rid]["id"] = rid;
			}
			
			records = nrec;
		}
		
		statstring idfield = "id";
		
		if (! records.count())
		{
			fout.printf ("%% \033[1mNo objects\033[0m\n");
			return 0;
		}

		// print header prt1
		value  vcolumns, vdata;
		
		formatter.clear ();
		vcolumns[idfield] = idfield;
		vcolumns[idfield]("bold") = "true";
		
		foreach (col, cclass.parameters ())
		{	
			if (col.id() != "id" && col["description"] != "")
			{
				bool addv = true;
				statstring cid = col.id();
				
				if (col["clihide"] == true) addv = false;
				
				if (addv)
				{
					vcolumns[cid] = cid;
					if (col["cliwidth"].ival())
					{
						vcolumns[cid]("fixwidth") = col["cliwidth"].ival();
					}
				}
			}
		}
		
		formatter.setcolumns (vcolumns);
	
		// Loop through records
		foreach (record, records)
		{
			vdata.newval();
			foreach (c, vcolumns)
			{
				statstring nickid = c.id();
				if (record.exists (nickid) && record[nickid].sval())
				{
					vdata[-1][c.id()] = record[nickid];
				}
			}
		}		
		
		formatter.adddata (vdata);
		formatter.go ();
	}

	return 0;
}

// ==========================================================================
// METHOD opencliApp::cmdCreate
// ==========================================================================
int opencliApp::cmdCreate (const value &argv)
{
	int startpos = 1; // Pointer to the start-of-parameters.
	statstring asuser;
	
	if (argv[1] == "as-user")
	{
		asuser = argv[2];
		startpos += 2;
	}
	
	value argvals; // The name=value arguments parsed back.
	string objid; // The object-id to pass, if any.
	statstring classid = shortnames[argv[startpos++]]("realid");
	coreclass cclass (conn, classid);
	
	if (cclass.singleton ())
	{
		objid = cclass.singletonid ();
	}
	else if (cclass.manualindex ())
	{
		objid = argv[startpos++];
	}
	
	cparam = conn.getparams (classid, "default");
	
	// extract data to a more usable kind
	for (int i=startpos; i<argv.count(); i++)
	{
		string val = argv[i];
		string key, grp;
		key = val.cutat ("=");
		val = val.stripchar ('"');
	
		argvals[key] = val;
	}

	if (objid) argvals["id"] = objid;

	// Check if we have all enabled required fields filled in
	foreach (pv, cparam["enabled"])
	{
		// If the parameter is required and has no default value
		if (pv["required"] == "true")
		{
			if (! argvals.exists (pv.id()))
			{
				// If there's a default value, we'll just put it in.
				if (pv["default"].sval())
				{
					argvals[pv.name()] = pv["default"].sval();
				}
				else
				{
					ferr.writeln ("%% Missing field: %s" %format (pv.id()));
					return 0;
				}
			}
		}
	}

	// We have to add all hidden fields to our argvals object
	foreach (v, cparam["other"])
	{
		if (v["required"])
		{
			argvals[v.id()] = v["default"].sval();
 		}
	}

	// Loop trough each field to check if the type is an enum, if so
	// we have to remap these to their right values
	foreach (v, argvals)
	{
		if (cclass.isenum (v.id()))
		{
			v = cclass.enumvalue (v.id(), v);
		}
	}
	
	// All required fields are present
	// TODO: use regexps from xml to validate the given data
	statstring parentid = ctx.uuid ();
	string nobjid;
	
	nobjid = conn.createobject (classid, argvals, parentid);
	
	if (! nobjid)
	{
		ferr.writeln ("%% No object created: %s" %format (conn.geterror()));
	}
	else
	{
		fout.writeln ("%% Object created with id: %s" %format (nobjid));
		if (asuser)
		{
			if (conn.chown (nobjid, asuser))
			{
				fout.writeln ("%% Owner set to %s" %format (asuser));
			}
			else
			{
				fout.writeln ("%% Error setting owner: %s"
											%format (conn.geterror()));
			}
		}
	}

	return 0;
}

int opencliApp::cmdSet (const value &argv)
{
	statstring classid = ctx.currentClass ();
	statstring parentid = ctx.parentuuid ();
	coreclass cclass (conn, classid);
	
	updateCommon (classid, ctx.id(), parentid, cclass, 1, argv);
	crecord = conn.selectrecord (ctx.id(), parentid, classid);
	return 0;
}

// ==========================================================================
// METHOD opencliApp::cmdUpdate
// ==========================================================================
int opencliApp::cmdUpdate (const value &argv)
{
	statstring parentid = ctx.uuid ();

	// Loop trough each field to check if the type is an enum, if so
	// we have to map these to their right values
	//value cinfo;
	statstring classid = shortnames[argv[1]]("realid");
	coreclass cclass (conn, classid);
		
	int paramstart = 3;
	string updateid = argv[2];
	
	if (cclass.singleton ())
	{
		paramstart = 2;
		updateid = cclass.singletonid ();
	}
	else if (cclass.autoindex ())
	{
		updateid = UUID.uuidfromtemp (updateid, classid, parentid);
	}
	
	if (cclass.metabase())
	{
		value trec = conn.getrecords (classid, parentid);
		value &rval = trec[updateid];
		classid = rval["class"];
	}
	
	updateCommon (classid, updateid, parentid, cclass, paramstart, argv);
	return 0;
}

// ==========================================================================
// METHOD opencliApp::updateCommon
// ==========================================================================
void opencliApp::updateCommon (const statstring &classid,
							   const statstring &id,
							   const statstring &parentid,
							   coreclass &cclass,
							   int paramstart,
							   const value &argv)
{
	value vpairs;
	
	// extract data to a more usable kind
	for (int i=paramstart; i<argv.count(); i++)
	{
		string val = argv[i];
		string key, grp;
		
		key = val.cutat ("=");
		val = val.stripchar ('"');
		vpairs[key] = val;
	}

	foreach (v, vpairs)
	{
		if (cclass.isenum (v.id())) v = cclass.enumvalue (v.id(), v);
	}
	
	bool res = conn.updateobject (classid, id, parentid, vpairs);
	if (! res)
	{
		fout.writeln ("%% Object not updated: %s" %format (conn.geterror()));
	}
	else
	{
		fout.writeln ("% Object updated");
	}
}

// ==========================================================================
// METHOD opencliApp::cmdDelete
// ==========================================================================
int opencliApp::cmdDelete (const value &argv)
{
	string objid;
	statstring classid = shortnames[argv[1]]("realid");
	statstring tpid = ctx.uuid ();
	coreclass cclass (conn, classid);
	
	if (argv.count() == 2) // No id-arguent given, can this be?
	{
		// No singleton-id provided in the class info, this is
		// fraud!
		if (! cclass.singleton ())
		{
			ferr.printf ("%% Incomplete command\n");
			return 0;
		}
		
		// Ok, resolve the implicit id.
		objid = cclass.singletonid ();
	}
	else // class and id-argument provided.
	{
		objid = argv[2];
		
		// If the class is auto-indexed, the user will have provided
		// an integer-key; resolve this back to its original UUID.
		if (cclass.autoindex ())
		{
			objid = UUID.uuidfromtemp (objid, classid, tpid);
		}
	}
	
	if (INTERACTIVE)
	{
		if (! formatter.userconfirm ("Do you really want to delete this item?"))
		{
			fout.printf ("Delete aborted...\n");
			return 0;
		}
	}

	// Perform the delete.
	if (cclass.metabase())
	{
		value trec = conn.getrecords (classid, ctx.parentuuid());
		value &tval = trec[objid];
		classid = tval["class"];
	}
	
	if (conn.deleteobject (classid, objid, ctx.uuid()))
	{
		fout.writeln ("%% Object '%s' removed" %format (argv[2]));
	}
	else
	{
		fout.writeln ("%% Error removing object: %s" %format (conn.geterror()));
	}

	return 0;
}

// ==========================================================================
// METHOD opencliApp::cmdSelectMeta
// ==========================================================================
int opencliApp::cmdSelectMeta (const value &argv)
{
	statstring metaclassid = shortnames[argv[1]]("realid");
	statstring metaobjectid = argv[2];
	statstring parentid = ctx.uuid ();
	statstring realobjectid;
	statstring realclassid;
	
	// Get the object list and classinfo to figure things out.
	value objlist = conn.getrecords (metaclassid, parentid);
	coreclass cclass (conn, metaclassid);
	
	// If The class uses autoindexing, we'll use a uuid-to-integer mapping.
	if (cclass.autoindex ())
	{
		realobjectid = UUID.uuidfromtemp (metaobjectid, metaclassid, parentid);
	}
	else realobjectid = metaobjectid;
	
	// Objects obtained out of a 'getrecords' call for a meta-baseclass
	// should be resolved back to their actual implementation class.
	// This class is kept inside the 'class' field.
	realclassid = objlist[realobjectid]["class"];
	if (! realclassid)
	{
		ferr.writeln ("% Error resolving object back to its class.");
		return 0;
	}
	
	// Get the classinfo and short name for the real class
	coreclass realclass (conn, realclassid);
	statstring realshort = realclass.shortname ();

	// Redo the select call with the resolved shortname and objectid.
	return cmdSelect ($(argv[1])->$(realshort)->$(realobjectid));
}

// ==========================================================================
// METHOD opencliApp::focusclass
// ==========================================================================
void opencliApp::focusclass (const statstring &newclass,
								 const statstring &newid,
								 const statstring &parentid,
								 coreclass &cclass)
{
	conn.setCurrentClass (newclass);
	shortnames.clear();
	foreach (vd, cclass.children ())
	{
		statstring nid = vd["shortname"];
		shortnames[nid] = vd["description"].sval();
		shortnames[nid]("realid") = vd["id"].sval();
	}
	
	if (newid)
	{
		crecord = conn.selectrecord (newid, parentid, newclass);
	}
	else
	{
		crecord.clear ();
	}
	
	statstring shortclass = cclass.shortname ();;

	if (newclass == "ROOT")
	{
		shell.setprompt ("[opencli]% ");
	}
	else if (cclass.singleton ())
	{
		shell.setprompt ("[%s]%% " %format (shortclass));
	}
	else
	{
		shell.setprompt ("[%s %s]%% " %format (shortclass, newid));
	}
}

// ==========================================================================
// METHOD opencliApp::cmdUpcontext
// ==========================================================================
int opencliApp::cmdUpcontext (const value &argv)
{
	// An 'exit' at top level is akin to a quit.
	if ((ctx.atRoot ()) && (argv[0] == "exit"))
	{
		return 1;
	}
	
	// If at 'root+1' or on the 'end' command, jump to ROOT.
	if ((argv[0] == "end") || (ctx.atFirstLevelOrRoot()))
	{
		coreclass C (conn, "ROOT");
		focusclass ("ROOT", "", "", C);
		ctx.root ();
	}
	else // jump up 1 level.
	{
		ctx.levelUp ();
		statstring newclass = ctx.currentClass ();
		statstring newid = ctx.id ();
		statstring parentid = ctx.parentuuid ();
		coreclass cclass (conn, newclass);
		focusclass (newclass, newid, parentid, cclass);
	}
	
	return 0;
}

// ==========================================================================
// METHOD opencliApp::cmdSelect
// ==========================================================================
int opencliApp::cmdSelect (const value &argv)
{
	statstring newclass; // actual class we will jump to
	statstring newid; // id we will jump to
	statstring parentid; // the parentid (if we're not in ROOT now)
	
	// convenience storage for parameters.
	statstring in_shortclass; 
	statstring in_id;
	
	parentid = ctx.uuid ();
	
	// Check argument count.
	if (argv.count() < 2)
	{
		ferr.writeln ("% Insufficient arguments");
		return 0;
	}
	
	in_shortclass = argv[1];
	newclass = shortnames[in_shortclass]("realid");
	coreclass cclass (conn, newclass);
	
	if (cclass.singleton ())
	{
		// Singletons have an implicit id so we can skip that
		// step.
		newid = cclass.singletonid ();
	}
	else
	{
		if (argv.count() < 3)
		{	
			ferr.writeln ("% Insufficient arguments");
			return 0;
		}
	
		// Special treatment for meta-base-classes. We'll divert these
		// to cmdSelectMeta(), which will call back cmdSelect() with
		// parameters rewritten to the actual class/(uu)id.
		if (cclass.metabase ())
		{
			return cmdSelectMeta (argv);
		}
		
		in_id = argv[2];
		
		// If indexing=auto, we have to resolve back the numeric local
		// id to a uuid
		if (cclass.autoindex ())
		{
			newid = UUID.uuidfromtemp (in_id, newclass, parentid);
		}
		else
		{
			// Ok things are much simpler.
			newid = in_id;
		}
		
		if (! newid)
		{
			ferr.writeln ("% No object-id found");
			return 0;
		}
	}
	
	focusclass (newclass, newid, parentid, cclass);

	value &R = crecord;
	ctx.enter (newclass, argv[1], cclass.description(), R["id"], R["uuid"]);
	
	return 0;
}

// ==========================================================================
// METHOD opencliApp::srcUserOrClassOrParam
// ==========================================================================
value *opencliApp::srcUserOrClassOrParam (const value &v, int p)
{
	// Non-root objects: just use srcClass/srcParam directly.
	if (! ctx.atRoot ())
	{
		if (p == 1) return srcClass (v, p);
		return srcParam (v, p);
	}
	
	if (p == 1)
	{
		returnclass (value) res retain;
		
		res["as-user"] = "Create object for a specific user";
		res << srcClass (v, p);
		return &res;
	}
	
	if (v[1] != "as-user") return srcParam (v, p);
	
	if (p == 2)
	{
		returnclass (value) res retain;
		value usrrecords = conn.getrecords ("User", "");
		foreach (usr, usrrecords)
		{
			res[usr.id()] = "";
		}
		return &res;
	}
	
	value newv;
	newv.newval() = v[0];
	for (int i=3; i<v.count(); ++i)
	{
		newv.newval() = v[i];
	}
	
	if (p == 3) return srcClass (newv, 1);
	return srcParam (newv, p-2);
}

// ==========================================================================
// METHOD opencliApp::srcClassObject
// ==========================================================================
value *opencliApp::srcClassObject (const value &v, int p)
{
	returnclass (value) res retain;

	if (p == 3)
	{
		ferr.printf ("%% Err: You can't select more than two parameters\n");
		return &res;
	}
	
	if (p == 1)
	{
		res = shortnames;
	}
	else
	{
		// return records that are available under the 
		// class, see 1st param
		statstring classid;
		if (! shortnames.exists (v[1].sval()))
		{
			foreach (sn, shortnames)
			{
				if (sn.id().sval().strncmp (v[1], v[1].sval().strlen()) == 0)
				{
					res[sn.id()] = sn;
				}
			}
			
			if (! res.count())
			{
				ferr.writeln ("%% Cannot expand '%s'" %format (v[1]));
			}
			return &res;
		}
		classid = shortnames[v[1]]("realid");
		statstring tpid = ctx.uuid ();
		value records = conn.getrecords (classid, tpid);
		coreclass cclass (conn, classid);
		
		if (! INTERACTIVE)
		{
			res[v[2].sval()];
			return &res;
		}
		
		if (cclass.singleton ())
		{
			return &res;
		}
		
		if (cclass.autoindex ())
		{
			value nrec;
			
			UUID.processlist (classid, tpid, records);
			
			foreach (trec, records)
			{
				statstring rid;
				rid = UUID.tempfromuuid (trec.id(), classid, tpid);
				nrec[rid] = trec;
				nrec[rid]["id"] = rid;
			}
			
			records = nrec;
		}
		
		// Loop through each record
		foreach (record, records)
		{
			res[record["id"]];
		}				
	}

	return &res;
}

// ==========================================================================
// METHOD opencliApp::srcClass
// ==========================================================================
value *opencliApp::srcClass (const value &v, int p)
{
	returnclass (value) res retain;
	
	statstring cmd = v[0];

	// create, update and delete commands are filtered
	if (cmd == "update" || cmd == "delete" || cmd == "exec")
	{
		foreach (w, shortnames)
		{
			coreclass C (conn, w("realid"));
			value cap;
			cap = conn.classcapabilities (w("realid"));
			
			// For metabase classes, we are actually interested in
			// the capabilities of any of the children. If they're
			// allowed, the metabase class is allowed. Derived children
			// themselves should be omitted from this list as they are
			// always manipulated through update/delete of the base
			// class.
			if (C.metabase())
			{
				foreach (childclass, C.metachildren())
				{
					cap = conn.classcapabilities (childclass);
					if (cap[cmd] == true) break;
				}
			}
			else if (C.metaderived()) continue;
			
			if (cap[cmd] == true || debug_or == true  || 
					cmd == "exec")
				res[w.id()] = w;
		}
	}
	else
	{
		foreach (w, shortnames)
		{
			coreclass C (conn, w("realid"));
			if (! C.metabase ())
			{
				res[w.id()] = w;
			}
		}
	}

	return &res;
}

// ==========================================================================
// METHOD opencliApp::srcClasses
// ==========================================================================
value *opencliApp::srcClasses (const value &v, int p)
{
	returnclass (value) res retain;

	if (allClasses.count() == 0)
		allClasses = conn.listclasses ();

	// Create return list with all available classes, using their original names
	// shortnames are out of the question cause they are not unique. 
	foreach (c, allClasses)
	{
		res[c.id()] = "(%[shortname]s), %[description]s" %format (c);
	}

	return &res;
}

// ==========================================================================
// METHOD opencliApp::srcField
// ==========================================================================
value *opencliApp::srcField (const value &v, int p)
{
	returnclass (value) res retain;
	
	// List all fields of the selected class
	// without the already chosen
	coreclass cclass (conn, shortnames[v[1]]("realid"));
	
	foreach (col, cclass.parameters ())
	{	
		if (! v.exists (col.id()))
			res[col.id()] = col["description"].sval();
	}	
	
	return &res;
}

// ==========================================================================
// METHOD opencliApp::srcParam
// ==========================================================================
value *opencliApp::srcParam (const value &v, int p)
{
	returnclass (value) res retain;

	statstring classid;
	statstring parentid;
	int startpos;
	value vpairs;
	statstring objid;
	coreclass cclass (conn);

	if (v[0] == "set")
	{
		startpos = 1;
		objid = ctx.id ();
		classid = ctx.currentClass ();
		parentid = ctx.parentuuid ();
		cclass.setclass (classid);
	}
	else
	{
		startpos = 3;
		parentid = ctx.uuid ();
		classid = shortnames[v[1]]("realid");
		cclass.setclass (classid);

		if (cclass.singleton ())
		{
			startpos=2;
			objid = cclass.singletonid ();
		}
		else if ((cclass.manualindex ()) && (v[0] == "create"))
		{
			startpos = 3;
			objid = v[2];
			
			if (p<startpos)
			{
				res["*"] = "New object id";
				return &res;
			}
		}
		else
		{
			if (v[0] == "create")
			{
				startpos = 2;
			}
			else
			{
				objid = v[2];
				if (cclass.autoindex ())
				{
					objid = UUID.uuidfromtemp (objid, classid, parentid);
				}
			}
		}

		if (cclass.metabase())
		{
			value recs = conn.getrecords (classid, parentid);
			value &vreal = recs[objid];
			classid = vreal["class"];
			cclass.setclass (classid);
		}

	}
	
	caseselector (v[0])
	{
		incaseof ("update") :
			// Update if the cached version does not match the
			// current request
			if (cparam["ofobject"].sval() != objid || 
				cparam["withparent"].sval() != ctx.uuid ())
			{
				cparam = conn.getparams (classid, "", objid, parentid);
			}
			break;
		
		incaseof ("create") :
			// Update if the cached version does not match the
			// current request
			if (cparam["class"].sval() != classid)
			{
				cparam = conn.getparams (classid, "default");
			}
			
			if ((! cclass.singleton ()) && (cclass.manualindex ()))
			{
				if (p == 2)
				{
					res["*"] = "New object id";
					return &res;
				}
			}
			break;
			
		incaseof ("set") :
			if (cparam["ofobject"].sval() != objid || 
				cparam["withparent"].sval() != ctx.uuid())
			{
				cparam = conn.getparams (classid, "", objid, parentid);
			}
			break;			
		
		defaultcase :
			ferr.printf ("Err: Internal error in srcParam\n");
			return &res;
	}
	
	// extract data to a more usable kind
	for (int i=startpos; i<p; i++)
	{
		string val = v[i];
		string key;
		
		key = val.cutat ("=");
		vpairs[key];
	}
	
	// Here we will create the actual expansion set .. yay
	// only show the enabled properties
	foreach (p, cparam["enabled"])
	{
		if (! vpairs.exists (p.id()))
		{
			string r;
			
			caseselector (p["type"])
			{
				incaseof ("bool") :
					r = "%s=true" %format (p.id());
					res[r] = p["description"].sval();
					r = "%s=false" %format (p.id());
					res [r] = p["description"].sval();
					break;
									
				incaseof ("enum") :
					value c = conn.classinfo (classid);
					foreach (ev, c["enums"][p.name()])
					{
						r = "%s=%s" %format (p.id(), ev.id());
						res[r] = ev.sval();
					}
					break;
				
				defaultcase :
					r = "%s=*" %format (p.id());
					res[r] = p["description"];
			}
		}
	}
	
	// You cant change the id field inside 
	// the update command, remove it
	if (res.exists ("id=*"))
		res.rmval ("id=*");
		
	return &res;
}

// ==========================================================================
// METHOD opencliApp::srcUser
// ==========================================================================
value *opencliApp::srcUser (const value &, int p)
{
	returnclass (value) res retain;

	value records = conn.getrecords ("User","");	
	foreach (record, records)
	{
		res[record["id"]];
	}

	return &res;
}

// ==========================================================================
// METHOD opencliApp::srcIdCommon
// ==========================================================================
void opencliApp::srcIdCommon (const value &v, statstring &classid, value &records)
{
	if (v.count() >= 1)
	{	
		classid = shortnames[v[1]]("realid");
		statstring tpid = ctx.uuid ();
		
		records = conn.getrecords (classid, tpid);
		coreclass cclass (conn, classid);
		
		if (cclass.autoindex ())
		{
			value nrec;
			
			UUID.processlist (classid, tpid, records);
			
			foreach (trec, records)
			{
				statstring rid;
				rid = UUID.tempfromuuid (trec.id(), classid, tpid);
				nrec[rid] = trec;
				nrec[rid]["id"] = rid;
			}
			
			records = nrec;
		}
	}
	else
	{
		// records 	= conn.getrecords (path[-2]["uuid"].sval());
		// tcls 		= conn.getCurrentClass();
	}
}

// ==========================================================================
// METHOD opencliApp::srcId
// ==========================================================================
value *opencliApp::srcId (const value &v, int p)
{
	returnclass (value) res retain;
	statstring classid;
	value records;

	srcIdCommon (v, classid, records);
	coreclass cclass (conn, classid);
	
	if (cclass.singleton ())
	{
		return &res;
	}
	
	// Loop through each record
	foreach (record, records)
	{
		res[record["id"]];
	}

	return &res;
}

// ==========================================================================
// METHOD opencliApp::srcIdParam
// ==========================================================================
value *opencliApp::srcIdParam (const value &v, int p)
{
	returnclass (value) res retain;
	statstring classid;
	value records;

	srcIdCommon (v, classid, records);
	coreclass cclass (conn, classid);
		
	if (cclass.singleton ())
	{
		res = srcParam (v, p);
		return &res;
	}
	
	// Loop through each record
	foreach (record, records)
	{
		res[record["id"]];
	}

	return &res;
}

// ==========================================================================
// METHOD opencliApp::srcMethods
// ==========================================================================
value *opencliApp::srcMethods (const value &v, int p)
{
	returnclass (value) res retain;
	
	coreclass C (conn, shortnames[v[1]]("realid"));
	foreach (method, C.methods ())
	{
		res[method.id()] = method["description"].sval();
	}
	
	return &res;
}

// ==========================================================================
// METHOD opencliApp::srcMethodparams
// ==========================================================================
value *opencliApp::srcMethodparams (const value &v, int p)
{
	returnclass (value) res retain;
	
	value parameters;
	parameters = conn.listmethodparams (v[3],v[1], ctx.uuid(), ctx.parentuuid());
	
	/// \todo Dynamic methods are not coherently implemented.
	return &res;
}

// ==========================================================================
// CONSTRUCTOR uuidmapper
// ==========================================================================
uuidmapper::uuidmapper (void)
{
}

// ==========================================================================
// DESTRUCTOR uuidmapper
// ==========================================================================
uuidmapper::~uuidmapper (void)
{
}

// ==========================================================================
// METHOD uuidmapper::processlist
// ==========================================================================
void uuidmapper::processlist (const statstring &ofclass,
							  const statstring &parentid,
							  const value &list)
{
	statstring p = parentid;
	if (! p) p = "@";
	uuid2temp[ofclass].rmval (p);
	temp2uuid[ofclass].rmval (p);
	
	value &vt = temp2uuid[ofclass][p];
	value &vu = uuid2temp[ofclass][p];
	
	int k = 1;
	foreach (node, list)
	{
		statstring tkey = "%i" %format (k);
		++k;
		vt[tkey] = node.id();
		vu[node.id()] = tkey;
	}
}

// ==========================================================================
// METHOD uuidmapper::uuidfromtemp
// ==========================================================================
const string &uuidmapper::uuidfromtemp (const statstring &id,
										const statstring &ofclass,
										const statstring &parentid)
{
	statstring p = parentid;
	if (! p) p = "@";
	return temp2uuid[ofclass][p][id].sval();
}

// ==========================================================================
// METHOD uuidmapper::tempfromuuid
// ==========================================================================
const string &uuidmapper::tempfromuuid (const statstring &uuid,
										const statstring &ofclass,
										const statstring &parentid)
{
	statstring p = parentid;
	if (! p) p = "@";
	return uuid2temp[ofclass][p][uuid].sval();
}
