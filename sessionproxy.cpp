// This file is part of OpenPanel - The Open Source Control Panel
// OpenPanel is free software: you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the Free 
// Software Foundation, using version 3 of the License.
//
// Please note that use of the OpenPanel trademark may be subject to additional 
// restrictions. For more information, please visit the Legal Information 
// section of the OpenPanel website on http://www.openpanel.com/

#include "sessionproxy.h"
#include <grace/http.h>
#include <grace/application.h>
#include <grace/sslsocket.h>
#include <grace/filesystem.h>

#include "opencli.h"

// ============================================================================
// METHOD sessionproxy::sessionproxy
// ============================================================================
sessionproxy::sessionproxy (void)
{
	url 		= "unix://[var:openpanel/sockets/openpanel-core.sock]/json";
	active 		= false;
	cclassid 	= "ROOT"; // default class on startup
}


// ============================================================================
// METHOD sessionproxy::~sessionproxy
// ============================================================================
sessionproxy::~sessionproxy (void)
{
	dropsession ();
}

void sessionproxy::seturl (const string &purl)
{
	url = purl;
}

// ============================================================================
// METHOD sessionproxy::dropsession
// ============================================================================
void sessionproxy::dropsession (void)
{
	if (active)
	{
		value req;
		value res;
		
		req["header"]["command"] = "logout";
		res = sendrequest (req);
		active = false;
	}
}

// ============================================================================
// METHOD sessionproxy::keepalive
// ============================================================================
void sessionproxy::keepalive (void)
{
	if (active)
	{
		value req, res;
		req["header"]["command"] = "ping";
		res = sendrequest (req);
	}
}

// ============================================================================
// METHOD sessionproxy::getsession
// ============================================================================
bool sessionproxy::getsession (const string &user, const string &pass)
{
	value req;
	value res;
	req["header"]["command"] = "bind";
	if (user.strlen())
	{
		req["body"]["classid"] = "User";
		req["body"]["id"] = user;
		req["body"]["data"]["id"] = pass;
	}
	
	res = sendrequest (req);
	if (! checkandhandle (res)) return false;
	
	id = res["header"]["session_id"];
	if (! id.strlen())
	{
		error = "Invalid session-id";
		return false;
	}
	
	authuser = user;

	// fetch root data
	setCurrentClass ("ROOT");
	
	active = true;
	return true;
}

// ============================================================================
// METHOD sessionproxy::createobject
// ============================================================================
string *sessionproxy::createobject (const string &ofclass, 
									const value &withdata,
									const string &inparent)
{
	// prmdata = parameter data created by getparams
	// inparent = the object it's parent uuid
	// withdata = a list of data
	// ofclass = class type

	returnclass (string) res retain;
	
	value request =
		$("header",
			$("command", "create")
		 ) ->
		$("body",
			$("classid", ofclass) ->
			$("parentid", inparent) ->
			$("data", withdata)
		 );

	if (withdata.exists("id"))
	{
		request["body"]["objectid"] = withdata["id"].sval();
		request["body"]["data"].rmval ("id");
	}

	value rpcresult;
	rpcresult = sendrequest (request);
	if (! checkandhandle (rpcresult)) return &res;
	objcache.clear ();
	
	res = rpcresult["body"]["data"]["objid"].sval();
	
	return &res;
}

// ============================================================================
// METHOD sessionproxy::deleteobject
// ============================================================================
bool sessionproxy::deleteobject (const string &ofclass, const string &withid,
								 const string &withparent)
{
	value result;
	value request =
		$("header",
			$("command", "delete")
		 ) ->
		$("body",
			$("classid", ofclass) ->
			$("objectid", withid) ->
			$("parentid", withparent)
		 );
	
	result = sendrequest (request);
	if (! checkandhandle (result)) return false;
	
	objcache.clear ();
	return true;
}

// ============================================================================
// METHOD sessionproxy::updateobject
// ============================================================================
bool sessionproxy::updateobject (const string &ofclass, const string &withid,
							   	 const string &withparent, 
							   	 const value &withparam)
{
	// prmdata = parameter data created by getparams
	// inparent = the object it's parent uuid
	// withdata = a list of data
	// ofclass = class type

	// First get the original record from the database
	value o_data = selectrecord  (withid, withparent, ofclass);

	o_data.rmval ("uuid");
	o_data.rmval ("id");
	o_data.rmval ("childclasses");
	o_data.rmval ("class");
	if (o_data.exists ("ownerid")) o_data.rmval ("ownerid");
	if (o_data.exists ("parentuuid")) o_data.rmval ("parentuuid");
	if (o_data.exists ("parentid")) o_data.rmval ("parentid");
	if (o_data.exists ("owneruuid")) o_data.rmval ("owneruuid");
	if (o_data.exists ("version")) o_data.rmval ("version");
	if (o_data.exists ("deleted")) o_data.rmval ("deleted");
	if (o_data.exists ("metaid")) o_data.rmval ("metaid");
	
	value request =
		$("header",
			$("command", "update")
		 ) ->
		$("body",
			$("classid", ofclass) ->
			$("parentid", withparent) ->
			$("objectid", withid) ->
			$("data", o_data)
		 );
	
	foreach (v, withparam)
		request["body"]["data"][v.id()] = v.sval();
		
	value result;
	
	result = sendrequest (request);
	if (! checkandhandle (result)) return false;
	
	objcache.clear ();
	return true;
}

// ============================================================================
// METHOD sessionproxy::classinfo
// ============================================================================
value *sessionproxy::classinfo (const string &ofclass)
{
	static value cache;
	returnclass (value) res retain;
	
	if (cache.exists (ofclass))
	{
		res = cache[ofclass];
	}
	else
	{
		value request;
		value rdat;
	
		request["header"]["command"] = "classinfo";
		request["body"]["classid"] = ofclass;
		
		rdat = sendrequest (request);
		if (! checkandhandle (rdat)) return &res;

		res = rdat["body"]["data"];
		cache[ofclass] = res;
	}
	return &res;
}

// ============================================================================
// METHOD sessionproxy::classcapabilities
// ============================================================================
value *sessionproxy::classcapabilities(const string &ofclass)
{
	returnclass (value) res retain;
	
	value tmp = classinfo (ofclass);
	res = tmp["capabilities"];
	
	return &res;
}

// ============================================================================
// METHOD sessionproxy::getparams
// ============================================================================
value *sessionproxy::getparams (const string &ofclass, const string &oflayout,
						const string &ofobject, const string &withparent)
{
	returnclass (value) res retain;

	// Create a value object wil all specified data extended with the
	// paramater values. This will return a vaue object with all 
	// necessary class data

	// Get class info and the selected layout
	value cdt = classinfo (ofclass);
	value prm = cdt["structure"]["parameters"];

//	used for debugging
//	prm.savexml ("params.bin");


	foreach (l, prm)
	{
		statstring node = l.id();
		statstring sect;

		// we create two seperate lists
		if (l["enabled"] == true)
		{
			// Enabled items need to filled by the user
			sect = "enabled";		
		}
		else if (l["required"] == true)
		{
			// if not enabled but required the create and update
			// functions need to know about it, the visual aspect is
			// not applicable here.. see l["visible"]
			sect = "other";
		}
	
		value &R = res[sect][node];
		value &P = prm[l.id()];
		
		R = l;
		R["description"] = P["description"];
		R["type"] = P["type"];
		if (P["type"] == "ref")
		{
			string rf = "%s/%s" %format (P["refclass"], P["reflabel"]);

			R["refstring"] = rf;
			R["refclass"] = P["refclass"];
			R["reflabel"] = P["reflabel"];
		}
	}
	
	// Set global properties
	res["class"] 		= ofclass;
	res["layout"]		= oflayout;
	res["ofobject"]		= ofobject;
	res["withparent"]	= withparent;

	//string fn = "/tmp/getparams-%S.xml" %format (ofclass);
	//res.savexml (fn);

	return &res;
}

// ============================================================================
// METHOD sessionproxy::getnamedlayout
// ============================================================================
value *sessionproxy::getnamedlayout (const string &ofclass, const string &oflayout)
{
	static value cache;
	returnclass (value) res retain;
	
	if (cache.exists (ofclass) && cache[ofclass].exists (oflayout))
	{
		res = cache[ofclass][oflayout];
		return &res;
	}
	
	value result;
	value request =
		$("header",
			$("command", "getlayout")
		 ) ->
		$("body",
			$("layoutid", oflayout) ->
			$("classid", ofclass)
		 );
	
	result = sendrequest (request);

	if (! checkandhandle (result)) return false;
	
	res = result["body"]["data"]["layout"]
				["options"]["layout"]["options"];
				
	cache[ofclass][oflayout] = res;
	return &res;
}

// ============================================================================
// METHOD sessionproxy::getlayout
// ============================================================================
value *sessionproxy::getlayout (const statstring &withid,
								const statstring &parentid,
								const statstring &ofclass)
{
	returnclass (value) res retain;
	
	value result;
	value request =
		$("header",
			$("command", "getlayout")
		 ) ->
		$("body",
			$("classid", ofclass ? ofclass : cclassid) ->
			$("objectid", withid) ->
			$("parentid", parentid)
		 );
	
	result = sendrequest (request);
	if (! checkandhandle (result)) return false;
	
	res = result["body"]["data"]["layout"]
				["options"]["layout"]["options"];
	
	return &res;
}

// ============================================================================
// METHOD sessionproxy::references
// ============================================================================
value *sessionproxy::getreferences (const string &refstring)
{
	returnclass (value) res retain;

	value request;
	value result;

	request["header"]["command"] = "getreferences";
	request["body"]["refstring"] = refstring;
	
	result = sendrequest (request);
	if (! checkandhandle (result))
	{
		res = false;
		return &res;
	}
	
	res = result["body"]["data"];
	
	return &res;
}

// ============================================================================
// METHOD sessionproxy::selectrecord
// ============================================================================
value *sessionproxy::selectrecord  (const statstring &withid,
									const statstring &parent,
									const statstring &ofclass)
{
	returnclass (value) res retain;
	
	value request =
		$("header",
			$("command", "getrecord")
		 ) ->
		$("body",
			$("classid", ofclass ? ofclass : cclassid) ->
			$("objectid", withid) ->
			$("parentid", parent)
		 );
	
	value tres = sendrequest (request);
	if (! checkandhandle (tres))
	{
		return &res;
	}
	
	
	res = tres["body"]["data"]["object"][0];
	
	// set new object id
	cobjectid = withid;
	return &res;
}

// ============================================================================
// METHOD sessionproxy::getrecords
// ============================================================================
value *sessionproxy::getrecords (const statstring &withclass, 
								 const statstring &parentid)
{
	returnclass (value) res retain;
	bool cached = false;
	
	if (objcache[parentid].exists (withclass))
	{
		value &C = objcache[parentid][withclass];
		time_t when = C("ctime").uval();
		time_t now = core.time.now ();
		int dif = now - when;
		if (dif > 60)
		{
			objcache[parentid].rmval (withclass);
		}
		else
		{
			res = objcache[parentid][withclass];
			res.rmattrib ("ctime");
			cached = true;
		}
	}
	
	if (! cached)
	{
		value request =
			$("header",
				$("command", "getrecords")
			 ) ->
			$("body",
				$("classid", withclass) ->
				$("parentid", parentid)
			 );
		
		value tres = sendrequest (request);
		if (! checkandhandle (tres)) return &res;
		
		res = tres["body"]["data"][withclass];
		
		if (withclass.sval().strncasecmp ("openpanel-core:", 9) != 0)
		{
			value &C = objcache[parentid][withclass];
			C = res;
			C("ctime") = (unsigned int) core.time.now ();
		}
	}
	
	coreclass C (*this, withclass);
	
	if (C.hasrefs ())
	{
		foreach (row, res)
		{
			C.resolverefs (row);
		}
	}
	
	if (! C.hasgroups())
	{
		return &res;
	}
	
	foreach (row, res)
	{
		C.groupify (row);
	}
	
	return &res;
}

// ============================================================================
// METHOD sessionproxy::checkandhandle
// ============================================================================
bool sessionproxy::checkandhandle (const value &res)
{
	if (! res.count())
	{
		error = "Connection error";
		throw connectionException();
	}
	
	int erid = res["header"]["errorid"];
	
	if (erid == 0x3000)
	{
		throw connectionException ("Session expired");
	}
	else if (erid)
	{
		error = res["header"]["error"];
		if (! error) error = "Error: %!" %format (res["header"]);
		return false;
	}
	return true;
}

// ============================================================================
// METHOD sessionproxy::listclasses
// ============================================================================
value *sessionproxy::listclasses (void)
{
	returnclass (value) res retain;
	value				request, result;

	request["header"]["command"] = "listclasses";
	
	result = sendrequest (request);
	if (! checkandhandle (result)) return false;
	
	res = result["body"]["data"]["classes"];
	return &res;
}

// ============================================================================
// METHOD sessionproxy::chown
// ============================================================================
bool sessionproxy::chown (const statstring &uuid, const statstring &owner)
{
	value res;
	value req =
		$("header",
			$("command", "chown")
		 ) ->
		$("body",
			$("objectid", uuid) ->
			$("userid", owner)
		 );
		 
	res = sendrequest (req);
	if (! checkandhandle (res)) return false;
	return true;
}

// ============================================================================
// METHOD sessionproxy::execmethod
// ============================================================================
bool sessionproxy::execmethod (const string &ofclass, const string &method,
							   const value  &withparam, const string &withparent,
							   const string &withid)
{
	value res;
	value request =
		$("header",
			$("command", "callmethod")
		 ) ->
		$("body",
			$("classid", ofclass) ->
			$("method", method) ->
			$("parentid", withparent)
		 );

	if (withid != "")
		request["body"]["id"] = withid;

	res = sendrequest (request);
	if (! checkandhandle (res)) return false;
	return true;
}

// ============================================================================
// METHOD sessionproxy::listmethodparams
// ============================================================================
value *sessionproxy::listmethodparams (const string &methodname,
									   const string &ofclass,
									   const string &withid,
									   const string &withparent)
{
	returnclass (value) res retain;
	value result;
	value request =
		$("header",
			$("command", "listparamsformethod")
		 ) ->
		$("body",
			$("classid", ofclass) ->
			$("method", methodname) ->
			$("parentid", withparent)
		 );

	if (withid) request["body"]["id"] = withid;

	result = sendrequest (request);
	if (! checkandhandle (result)) return false;

	// copy ; todo: adjust output
	res = result;

	return &res;
}

// ============================================================================
// METHOD sessionproxy::sendrequest
// ============================================================================
value *sessionproxy::sendrequest (const value &req)
{
	returnclass (value) res retain;
	value outreq = req;
	string origin;
	
	if (! connectionsrc.strlen()) connectionsrc = "vty";
	origin = "openpanel-cli/pid=%i/src=%s" %format ((int)kernel.proc.self(),
															connectionsrc);
	if (id.strlen())
	{
		outreq["header"]["session_id"] = id;
	}
	
	httpsocket ht;
	ht.postheaders["X-OpenCORE-Origin"] = origin;
	
	if (url.strncmp ("https://", 8) == 0)
	{
		ht.sock().codec = new sslclientcodec;
		ht.sock().codec->nocertcheck ();
	}
	
	string jsonout;
	string jsonin;
	
	jsonin = outreq.tojson ();
	jsonout = ht.post (url, "application/json", jsonin);
	
	if ((ht.status != 200) || (! jsonout))
	{
		string realst = "%i" %format (ht.status);
		if (ht.status != 200) fs.save ("realstatus",realst);
		delete &res;
		if (ht.status != 200) throw (connectionException("no 200 status"));
		throw (connectionException("No JSON data"));
	}
	
	res.fromjson (jsonout);
	if (! res.count())
	{
		throw (connectionException("Could not parse JSON data"));
	}
	return &res;
}
