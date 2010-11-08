// This file is part of OpenPanel - The Open Source Control Panel
// The Grace library is free software: you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the Free 
// Software Foundation, either version 3 of the License.
//
// Please note that use of the OpenPanel trademark may be subject to additional 
// restrictions. For more information, please visit the Legal Information 
// section of the OpenPanel website on http://www.openpanel.com/

#ifndef _CLIENT_SESSIONPROXY_H
#define _CLIENT_SESSIONPROXY_H 1

#include <grace/exception.h>

$exception (connectionException,"Connection error");
extern bool SHOULDEXIT;

enum proxyException
{
	EX_PROXY_ICKY_ERROR	= 0xe0457d89
};

#include <grace/value.h>

/// Proxy class for keeping session state and querying opencore data.
class sessionproxy
{
public:
					 /// Constructor.
					 sessionproxy (void);
					 
					 /// Destructor
					~sessionproxy (void);
					
					 /// Change the connection url.
	void			 seturl (const string &);
	
					 /// Set the 'source' attribute for informational
					 /// purposes.
	void			 setsource (const string &s) { connectionsrc = s; }
	
					 /// Attempt to create a session on the back-end.
					 /// \param username The opencore username.
					 /// \param pass The opencore password.
	bool			 getsession (const string &username, const string &pass);
	
					 /// Log out the session.
	void			 dropsession (void);
	
					 /// Send a keep-alive ping command to prevent the
					 /// session from timing out.
	void			 keepalive (void);

					 /// Returns true if the current session is logged
					 /// in and available.
	bool			 isactive (void) { return active; }
	
					 /// Returns the username we used for authenticating
					 /// the session.
	const string	&getauthuser (void) { return authuser; }
	
					 /// Create a new object.
					 /// \param ofclass Object's classid.
					 /// \param withdata The data structure to use for creating
					 ///                 an object. If an explicit id is
					 ///                 needed for the object,it should be
					 ///                 passed as withdata["id"], this function
					 ///                 will rewrite that to /body/objectid.
					 /// \param inparent The parentid.
					 /// \return The uuid of the object created.
	string			*createobject (const string &ofclass, const value &withdata,
								   const string &inparent);

					 /// Delete an object.
					 /// \param ofclass The object's class name.
					 /// \param withid The object's uuid/metaid.
					 /// \param withparent The object's parentid.
					 /// \return True, if delete succeeded.
	bool			 deleteobject (const string &ofclass, 
								   const string &withid,
								   const string &withparent);
								   
					 /// Update an existing object.
					 /// \param ofclass The object's class name.
					 /// \param withid The object's uuid/metaid.
					 /// \param withparent The object's parentid.
					 /// \param withparam Update parameters.
					 /// \return True, if update succeeded.
	bool			 updateobject (const string &ofclass, 
								   const string &withid,
								   const string &withparent, 
								   const value &withparam);

					 /// Returns a dictionary indexed by classname
					 /// containing a 'shoartname', 'description' and
					 /// 'version' field.
	value			*listclasses (void);

					 /// Get layout information for a specific instance
					 /// of an object.
					 /// \param withid The object's uuid/metaid.
					 /// \param parentid The object's parentid.
					 /// \param ofclass The object's class, if empty
					 ///                this will default to the
					 ///                class in the session's current
					 ///                context.
					 /// \return An dictionary indexed by fieldname
					 ///         containing visibility elements:
					 ///         enabled, visible, regexp, required,
					 ///         example or default.
					 /// \todo This function should be obsoleted.
	value			*getlayout (const statstring &withid, 
								const statstring &parentid,
								const statstring &ofclass="");
								 
					 /// Get layout information by name.
					 /// \param ofclass The class name.
					 /// \param oflayout The specific layout name.
					 /// \return A layout in the style of
					 ///         sessionproxy::getlayout().
					 /// \todo This method is senseless, the only
					 ///       possible layout right now is 'default'.
	value 			*getnamedlayout (const string &ofclass, 
								     const string &oflayout);

					 /// Call a class or object method.
					 /// \param ofclass The class name.
					 /// \param method The method name.
					 /// \param withparam Function parameters.
					 /// \param withparent If applicable, a parentid.
					 /// \param withid If applicable, an specific object id.
					 /// \return True if method call succeeded.
	bool			 execmethod (const string &ofclass,
								 const string &method,
								 const value  &withparam,
								 const string &withparent,
								 const string &withid="");

					 /// Get a dynamic list of parameters for a 
					 /// method call on a specific objectinstance.
					 /// \param methodname The method to call
					 /// \param ofclass The class name.
					 /// \param withid The instance-id.
					 /// \param withparent The parent-id.
					 /// \return God knows. No module implements this yet.
					 /// \todo Rip this out until it is well-defined.
	value			*listmethodparams ( const string &methodname,
										const string &ofclass,
										const string &withid,
										const string &withparent);
	
					 /// Get a somewhat elaborate reading of object/class
					 /// parameters. Inside /enabled/$fieldname you
					 /// will find the following items:
					 /// - enabled (bool)
					 /// - visible (bool)
					 /// - regexp (string)
					 /// - example (string)
					 /// - default (string)
					 /// - description (string)
					 /// - type (string)
					 /// 
					 /// Other items inside the root of the returned
					 /// dictionary are:
					 /// - class (string)
					 /// - layout (string (always "default"))
					 /// - ofobject (string)
					 /// - withparent (string)
					 /// 
					 /// It's unclear why these are returned.
					 /// \param ofclass The class name
					 /// \param oflayout The layout (always 'default')
					 /// \param ofobject Optional object-id.
					 /// \param withparent Optional parent-id.
	value			*getparams	(const string &ofclass, 
								 const string &oflayout,
								 const string &ofobject="",
								 const string &withparent="");
	
					 /// Get a list of references according to a
					 /// "ClassName/fieldname" refstring.
					 /// \param refstring The refstring.
					 /// \return The result, oddly enough in RPC format with
					 ///         its return array in /body/data.
	value			*getreferences (const string &refstring);
	
					 /// Attempt to get a single record out of opencore.
					 /// This also changes the flag for 'current object id'
					 /// kept in sessionproxy::cobjectid.
					 /// \param withid The objectid
					 /// \param parent The parent-id
					 /// \param ofclass The class name.
					 /// \return The result in RPC format.
	value			*selectrecord  (const statstring &withid,
									const statstring &parent,
									const statstring &ofclass="");
									
					 /// Get a list of all records of a provided class
					 /// with a given parentid. Uses/updates the
					 /// sessionproxy::objcache value object to keep track
					 /// of earlier results.
	value 			*getrecords (const statstring &withclass,
								 const statstring &parentid);
	
					 /// Change a root-object's owner.
	bool			 chown (const statstring &uuid,
							const statstring &ownerid);


					 /// Set the current class focus.
					 /// \param s The class name.
	bool			 setCurrentClass (const string &s)
					 {
					 	objcache.clear ();
					 	_class 		= classinfo (s);
					 	cclassid	= s;
					 	return true;
					 }
					 
					 /// Get currently active object data
	value			&getCurrentObject (void)
					 {
						return _object;
					 }
					 
					 /// Get currently active class data
	value			&getCurrentClass (void)
					 {
					 	return _class;
					 }

					 /// Cached RPC classinfo call.
					 /// Uses sessionproxy::cache to save server
					 /// roundtrips.
					 /// \param ofclass The class name.
					 /// \return The classinfo structure.
	value			*classinfo 			(const string &ofclass);
	
					 /// Get the capabilities flags for a specific
					 /// class.
					 /// \param ofclass The class name.
	value			*classcapabilities	(const string &ofclass);
	
					 /// Get the last recorded error string.
	string 			&geterror (void)
					 {
					 	return error;	
					 }
	
					 /// Hand off an RPC request directly.
					 /// \param v RPC parameters.
					 /// \return The rpc return data.
	value			*sendrawrequest (const value &v)
					 {
					 	return sendrequest (v);
					 }
	
protected:

					 /// Cache of objects, indexed by
					 /// /$parentid/$classid
	value			 objcache;
	
					 /// Performs a RPC request.
	value			*sendrequest 	(const value &);
	
					 /// Check RPC result set for errors.
	bool			 checkandhandle (const value &);

	bool			 active;		///< True if a session is active.
	
	value			 _object;		///< current object data
	value			 _class;		///< current class data
	
	statstring		 cclassid;		///< current class id
	statstring		 cobjectid;		///< current object id
	
	string			 id;			///< session id
	string			 url;			///< connection url
	string			 error;			///< error string
	string			 connectionsrc;	///< connection source
	string			 authuser;		///< authenticated user
};

#endif
