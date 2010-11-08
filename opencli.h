// This file is part of OpenPanel - The Open Source Control Panel
// OpenPanel is free software: you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the Free 
// Software Foundation, using version 3 of the License.
//
// Please note that use of the OpenPanel trademark may be subject to additional 
// restrictions. For more information, please visit the Legal Information 
// section of the OpenPanel website on http://www.openpanel.com/

#ifndef _opencli_H
#define _opencli_H 1
#include <grace/application.h>
#include <grace/terminal.h>

#include "sessionproxy.h"
#include "textformat.h"

extern bool SHOULDEXIT;

/// Utility class that keeps track of the cli's current 'path' inside the
/// object tree. 
class pathcontext
{
public:
	const string		&parentuuid (void) const
						 {
						 	if (path.count() < 3) return emptystring;
						 	return path[-3]["uuid"].sval();
						 }
	
	const string		&currentClass (void) const
						 {
						 	return path[-1]["class"].sval();
						 }
	
	const string		&id (void) const
						 {
						 	return path[-1]["id"].sval();
						 }
	
	const string		&uuid (void) const
						 {
						 	return path[-1]["uuid"].sval();
						 }
						 
	void				 root (void)
						 {
							prevpath = path;
						 	path = $( $("id","ROOT") ->
						 			  $("type", "object") ->
						 			  $("class", "root") );
						 }
						 
	bool				 atRoot (void) const
						 {
						 	return (path.count() == 1);
						 }
						 
	bool				 atFirstLevelOrRoot (void) const
						 {
						 	return (path.count() < 4);
						 }
						 
	void				 levelUp (void)
						 {
						 	prevpath = path;
							
							if (path.count()<3) return;
						 	path.rmindex (path.count()-1);
						 	path.rmindex (path.count()-1);
						 }

	bool				 back (void)
						 {
							if ( prevpath.count()>= 1 )
							{
								value tmp = prevpath;
						 		prevpath = path;
								path = tmp;
								return true;
							}
							else
							{
								return false;
							}
						 }
						 
	void				 enter (const statstring &newclass,
								const statstring &shortname,
								const statstring &description,
								const statstring &objectid,
								const statstring &objectuuid)
						 {
							prevpath = path;
						 	path.newval() = $("id", newclass) ->
						 					$("short", shortname) ->
						 					$("description", description) ->
						 					$("type", "class") ->
						 					$("class", newclass);
						 	
						 	path.newval() = $("id", objectid) ->
						 					$("uuid", objectuuid) ->
						 					$("type", "object") ->
						 					$("class", newclass);
						 }
						 
protected:
	value				 path;
	value				 prevpath;
};

//  -------------------------------------------------------------------------
/// Main application class.
//  -------------------------------------------------------------------------
/// Keepalive callback.
void cbkeepalive (void *obj);

/// Utility class for mapping the uuid primary key of objects of a class
/// with indexing=auto to a more friendly local integer key.
class uuidmapper
{
public:
					 /// Boring constructor.
					 uuidmapper (void);
					 
					 /// Boring destructor.
					~uuidmapper (void);

					 /// Process a list of objects of a particular
					 /// class/parent combination. The old uuid cache
					 /// for that combination will be replaced by
					 /// a new one.
					 /// \param ofclass The class name.
					 /// \param parentid The parent-id.
					 /// \param list The object list, should be a
					 ///             dictionary keyed by uuids.
	void			 processlist (const statstring &ofclass,
								  const statstring &parentid,
								  const value &list);
	
					 /// Resolve a cached local integer to its
					 /// origin UUID.
					 /// \param id The 'integer' id.
					 /// \param ofclass The class name.
					 /// \param parentid The parent-id.
	const string	&uuidfromtemp (const statstring &id,
								   const statstring &ofclass,
								   const statstring &parentid = "");
	
					 /// Resolve a uuid to its local 'integer' id.
					 /// Obviously this only works for combinations
					 /// of class/parentid that have gone through
					 /// processlist() before.
					 /// \param uuid The uuid to resolve.
					 /// \param ofclass The class name.
					 /// \param parentid The parent-id.
	const string	&tempfromuuid (const statstring &uuid,
								   const statstring &ofclass,
								   const statstring &parentid = "");
								   
protected:
	value			 uuid2temp; ///< The cache, keyed by uuid.
	value			 temp2uuid; ///< The cache, keyed by 'integer'.
};

/// Main application object.
class opencliApp : public application
{
public:
						 /// Constructor.
						 /// Set up rpc connection, shell binding and
						 /// command line parameters.
						 opencliApp (void) :
							application ("opencli"),
							//conn ("http://father:4088/json"),
							shell (this, fin, fout),
							formatter (&fout, &fin, this)
						 {
							debug_or = false;
							
							opt = $("-l", $("long", "--login")) ->
								  $("--login",
								      $("argc", 0) ->
								      $("help", "Authenticate based on "
								      			"username/password")
								   ) ->
								  $("-c", $("long", "--shellcmd")) ->
								  $("--shellcmd",
								      $("argc", 1) ->
								      $("help", "Process commands "
								      			"like a shell")
								   ) ->
								  $("-h", $("long", "--host")) ->
								  $("--host",
								      $("argc", 1) ->
								      $("help", "External host address")
								   );
						 }
						 
						 /// Destructor.
						~opencliApp (void)
						 {
						 }

						 /// Send a keepalive ping to opencore.
	void				 keepalive (void);
	
						 /// Main application object.
	int					 main (void);

						 /// Execute the opencli command line.
	void				 commandline (void);

						 /// Implementation of the 'show' command with
						 /// no arguments.
	int					 cmdShow (const value &argv);

						 /// Implementation of the 'show' command with
						 /// a specific object class argument. Displays
						 /// a list of records.
	int					 cmdList (const value &argv);
	
	int					 cmdSelectMeta (const value &argv);
	
						 /// Implementation of the 'configure' command.
	int					 cmdSelect (const value &argv);
	
	void				 focusclass (const statstring &newclass,
									 const statstring &newid,
									 const statstring &parentid,
									 class coreclass &c);
	
	int					 cmdUpcontext (const value &argv);
	int					 cmdBackcontext (const value &argv);
	
						 /// Implementation of the 'delete' command.
	int					 cmdDelete (const value &argv);
	
						 /// Implementation of the 'create' command. 
	int					 cmdCreate (const value &argv);
	
						 /// Implementation of the 'update' command.
	int					 cmdUpdate (const value &argv);
	
						 /// Implementation of the 'set' command.
	int					 cmdSet (const value &argv);
	
	void				 updateCommon (const statstring &classid,
									   const statstring &id,
									   const statstring &parentid,
									   class coreclass &cl,
									   int paramstart,
									   const value &argv);
	
						 /// Show basic help information.
	int					 cmdHelp (const value &argv);
	
						 /// Stub-function that also doesn't return
						 /// anything.
						 /// \todo Rip this out or figure out the 'why'.
	int					 cmdUp (const value &argv) {};
	
						 /// Implementation of the 'exit' command.
	int					 cmdExit (const value &argv) { return 1; }

						 /// Implementation of the 'debug' command.
	int					 cmdDebug (const value &argv);
	
						 /// Implementation of the 'query' command.
						 /// \todo No comprende.
	int					 cmdQuery (const value &argv);
	
						 /// Implementation of the 'exec' command.
	int					 cmdMethod (const value &argv);
	
						 /// Implementation of the 'version' command.
	int					 cmdVersion (const value &argv);	
	
						 /// Source for classname expansion in the
						 /// create, update, delete, exec context.
						 /// \param v The split command line.
						 /// \param p The cursor position.
	value				*srcClass (const value &v, int p);
	
						 /// Source for expansion in the create
						 /// context: In case of root objects
						 /// there is an optional initial
						 /// "as-user" parameter if there are
						 /// extra users.
						 /// \param v The split command line.
						 /// \param p The cursor position.
	value				*srcUserOrClassOrParam (const value &v, int p);
	
						 /// Expansion source for all possible
						 /// classes regardless of context.
						 /// \param v The split command line.
						 /// \param p The cursor position.
	value				*srcClasses	(const value &v, int p);
	
						 /// Expansion source of field names for
						 /// a class used by the somewhat unclear
						 /// 'query' command.
						 /// \param v The split command line.
						 /// \param p The cursor position.
	value				*srcField (const value &v, int p);
	
						 /// Expansion source for the combination
						 /// "classname objectid" or just plain
						 /// "classname" in the case of a
						 /// singleton. It will always expand
						 /// possible classnames in v[1]. Non-
						 /// singleton objects will trigger an
						 /// expansion of an instance-id in v[2].
						 /// \param v The split command line.
						 /// \param p The cursor position.
	value				*srcClassObject (const value &v, int p);
	
						 /// Expansion source for an instance-id in the
						 /// 'exec' command context.
						 /// \param v The split command line.
						 /// \param p The cursor position.
	value				*srcId (const value &v, int p);
	
						 /// Expansion of either an explicit id, or
						 /// a hand-off to srcParam if the class
						 /// in v[1] is of the singleton type.
						 /// \param v The split command line.
						 /// \param p The cursor position.
	value				*srcIdParam (const value &v, int p);

						 /// Expansion of "name=value" pairs in context
						 /// of the class in v[1].
						 /// \param v The split command line.
						 /// \param p The cursor position.
	value				*srcParam (const value &v, int p);
	
						 /// Returns a static expansion list for setting
						 /// permission bits.
						 /// \param v The split command line.
						 /// \param p The cursor position.
	value				*srcPermission (const value &v, int p);
	
						 /// Expansion source for the User list.
						 /// \param v The split command line.
						 /// \param p The cursor position.
	value				*srcUser (const value &v, int p);
	
						 /// Expansion source for object method
						 /// selection in the 'exec' command.
	value				*srcMethods (const value &v, int p);
	
						 /// Expansion source for object method
						 /// parameters in the 'exec' command.
						 /// \param v The split command line.
						 /// \param p The cursor position.
	value				*srcMethodparams (const value &v, int p);
	
	cli<opencliApp>	 shell; ///< Bound grace command line parser.

protected:
	void				 srcIdCommon (const value &, statstring &, value &);
	
	string				*uuidToUser (const statstring &uuid);
	
	sessionproxy		 conn; ///< Connection class for opencore

	pathcontext			 ctx; ///< Internal stack for our object path.
	
						 /// A context-bound dictionary, indexed by
						 /// 'shortname' that keeps track of the
						 /// associated OpenCORE classnames and
						 /// friendly description for such cli-specific
						 /// shortened classnames. Each node has the
						 /// description as its value and an attribute
						 /// 'realid' containing the classid.
	value				 shortnames;
	
	value				 allClasses; ///< Population of all known classes
	value				 crecord;	///< current record's data
	value				 cparam;	///< cached parameters
	
	textformatter		 formatter;	
	string 				 prom; ///< Command prompt.
	bool				 debug_or; 	///< ignore perm flags when set true
	uuidmapper			 UUID; ///< uuid to temp key mapping.
};

/// Utility class helping with introspection of an OpenCORE class.
class coreclass
{
public:
					 coreclass (sessionproxy &p) : conn (p)
					 {
					 }
					 
					 coreclass (sessionproxy &p, const statstring &id)
					 	: conn (p)
					 {
					 	setclass (id);
					 }
					 
					~coreclass (void) {}
					
	void			 setclass (const statstring &id)
					 {
					 	cinfo = conn.classinfo (id);
					 }
					 
	bool			 autoindex (void)
					 {
					 	return (cinfo["class"]["indexing"] == "auto");
					 }
					 
	bool			 manualindex (void)
					 {
					 	return (! autoindex ());
					 }
	
	bool			 singleton (void)
					 {
					 	return (bool) cinfo["class"]["singleton"].sval();
					 }
					 
	bool			 metabase (void)
					 {
					 	return (cinfo["class"]["metatype"] == "base");
					 }
					 
	bool			 metaderived (void)
					 {
					 	return (cinfo["class"]["metatype"] == "derived");
					 }
	
	const value		&metachildren (void)
					 {
					 	return cinfo["class"]["metachildren"];
					 }
					 
	bool			 isenum (const statstring &param)
					 {
					 	return (cinfo["structure"]["parameters"]
					 				 [param.id()]["type"] == "enum");
					 }
					 
	bool			 hasrefs (void)
					 {
					 	foreach (p, cinfo["structure"]["parameters"])
					 	{
					 		if (p["type"] == "ref") return true;
					 	}
					 	return false;
					 }
					 
	bool			 hasgroups (void)
					 {
					 	foreach (p, cinfo["structure"]["parameters"])
					 	{
					 		if (! p.exists ("group")) continue;
					 		if (p["group"].sval()) return true;
					 	}
					 	return false;
					 }
					 
	void			 groupify (value &dat)
					 {
					 	value ndat = dat;
					 	value &cparam = cinfo["structure"]["parameters"];
					 	foreach (node, ndat)
					 	{
					 		if (! cparam.exists (node.id())) continue;
					 		if (! cparam[node.id()].exists ("nick")) continue;
					 		statstring gr = cparam[node.id()]["nick"];
					 		
					 		dat[gr] = dat[node.id()];
					 		dat.rmval (node.id());
					 	}
					 }
					 
	void			 resolverefs (value &dat)
					 {
					 	value &cparam = cinfo["structure"]["parameters"];
					 	foreach (node, dat)
					 	{
					 		if (! cparam.exists (node.id())) continue;
					 		value &p = cparam[node.id()];
					 		if (p["type"] != "ref") continue;
					 		
					 		value req = $("header",
					 						$("command", "getrecord")
					 					 )->
					 					$("body",
					 						$("classid", p["refclass"]) ->
					 						$("objectid", node.sval())
					 					 );
					 		
					 		value res = conn.sendrawrequest (req);
					 		if (res["body"]["data"].exists ("object"))
					 		{
					 			node = res["body"]["data"]
					 					  ["object"][0][p["reflabel"]];
					 		}
					 	}
					 }
					 
	const string	&enumvalue (const statstring &id, const statstring &v)
					 {
					 	return cinfo["enums"][id][v]["val"];
					 }
					 
	const value		&singletonid (void)
					 {
					 	return cinfo["class"]["singleton"];
					 }
					 
	const value		&description (void)
					 {
					 	return cinfo["class"]["description"];
					 }
					 
	const string	&shortname (void)
					 {
					 	return cinfo["class"]["shortname"];
					 }
	const value		&children (void)
					 {
					 	return cinfo["info"]["children"];
					 }
	
	const value		&parameters (void)
					 {
					 	return cinfo["structure"]["parameters"];
					 }
					 
	const value		&methods (void)
					 {
					 	return cinfo["structure"]["methods"];
					 }
	
	
protected:
	value			 cinfo;
	sessionproxy	&conn;
};

#endif
