/*
 *
 *  D-Bus++ - C++ bindings for D-Bus
 *
 *  Copyright (C) 2005-2007  Paolo Durante <shackan@gmail.com>
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctemplate/template.h>

#include "generator_utils.h"
#include "generate_stubs.h"

using namespace ctemplate;
using namespace std;
using namespace DBus;

/*! Generate RPC stub code for an XML introspection
*/

/*! Generate the code for the methods in the introspection file.
 * Each call to this function can generate code for either the
 * synchronous, blocking versions of the method invocations, or
 * for the asynchronous, non-blocking versions.
 */
void generate_methods(TemplateDictionary *dict, Xml::Nodes &methods)
{
	// this loop generates all methods
	for (Xml::Nodes::iterator mi = methods.begin(); mi != methods.end(); ++mi)
	{
		Xml::Node &method = **mi;
		Xml::Nodes args = method["arg"];
		Xml::Nodes args_in = args.select("direction", "in");
		Xml::Nodes args_out = args.select("direction", "out");
		string name(method.get("name"));

		TemplateDictionary *sync_method_dict =
				dict->AddSectionDictionary("FOR_EACH_METHOD");
		TemplateDictionary *async_method_dict =
				dict->AddSectionDictionary("FOR_EACH_ASYNC_METHOD");
		TemplateDictionary *method_dicts[] = {
			sync_method_dict,
			async_method_dict
		};
                TemplateDictionary *ret_type_dict = nullptr;
		int m;
		for (m = 0; m < 2; m++)
			method_dicts[m]->SetValue("METHOD_NAME", legalize(name));
		if (args_out.size() == 1)
                {
                        ret_type_dict = sync_method_dict->AddSectionDictionary("METHOD_HAS_RETURN_TYPE");
			sync_method_dict->SetValue("METHOD_RETURN_TYPE",
						   signature_to_type(args_out.front()->get("type")));
                        // TODO: Merge std::vector and DBus::Struct as they have something in common?
                        Ty type = signature_to_ty(args_out.front()->get("type"));
                        if (type.ident == "std::vector")
                        {
                                assert(ret_type_dict);
                                // Just added so that we can enter the section
                                sync_method_dict->ShowSection("FOR_EACH_RET_VEC");
                                // We assume maximum nesting depth of 1 for tplParams..
                                if (type.tplParams.size() == 1)
                                {
                                        // We have one easy type
                                        if (type.tplParams[0].tplParams.size() == 0)
                                        {
                                                ret_type_dict->ShowSection("VEC_SINGLE_TYPE");
                                                ret_type_dict->SetValue("VEC_TYPE", type.tplParams[0].ident);
                                        }
                                        else // We probably have a DBus::Struct or similar
                                        {
                                                ret_type_dict->ShowSection("VEC_MULTI_TYPE");
                                                ret_type_dict->SetValue("VEC_TYPE", type.tplParams[0].str());
                                                int cnt = 0;
                                                for (Ty const &t : type.tplParams[0].tplParams)
                                                {
                                                        TemplateDictionary *vec_types_dict = ret_type_dict->AddSectionDictionary("VEC_TYPES");
                                                        vec_types_dict->SetValue("VEC_TYPE", t.ident);
                                                        vec_types_dict->SetIntValue("VEC_COUNT", cnt);
                                                        ++cnt;
                                                }
                                        }
                                }
                        }
                        else if (type.ident == "std::map")
                        {
                                if (type.tplParams.size() != 2)
                                {
                                        cerr << "A std::map with " << type.tplParams.size()
                                             << " parameters when 2 are expected: " << type
                                             << ", for signature: " << args_out.front()->get("type")
                                             << endl;
                                        exit(-1);
                                }
                                // We only support simple mappings of basic type -> basic type and also
                                // basic type -> variant type
                                if (type.tplParams[0].ident == "::DBus::Struct"
                                    || type.tplParams[1].ident == "::DBus::Struct")
                                {
                                        cerr << "DBus::Struct as key or value in std::map is not supported" << endl;
                                        exit(-1);
                                }
                                // Add it so that we can enter the section
                                sync_method_dict->ShowSection("FOR_EACH_RET_MAP");
                        }
                        else if (type.ident == "::DBus::Struct")
                        {
                                assert(ret_type_dict);
                                sync_method_dict->ShowSection("FOR_EACH_RET_STRUCT");
                                int cnt = 1; // We start at 1 so that we can access the struct
                                             // members easily
                                for (Ty const& t : type.tplParams)
                                {
                                        if (t.tplParams.size() != 0)
                                        {
                                                cerr << "Can't handle ::DBus::Struct with members that have template parameters" << endl;
                                                cerr << "Type in question: " << type << endl;
                                                exit(-1);
                                        }
                                        TemplateDictionary *struct_types_dict = sync_method_dict->AddSectionDictionary("STRUCT_TYPES");
                                        struct_types_dict->SetValue("STRUCT_TYPE", t.ident);
                                        struct_types_dict->SetIntValue("STRUCT_CNT", cnt);
                                        ++cnt;
                                }
                        }
                        else
                        {
                                sync_method_dict->ShowSection("FOR_EACH_RET_PRIM");
                        }
                }
		else
                {
			sync_method_dict->SetValue("METHOD_RETURN_TYPE", "void");
                }
                
		// generate all 'in' arguments for a method signature
		if (args_in.size() > 0)
		{
			for (m = 0; m < 2; m++)
				method_dicts[m]->ShowSection("METHOD_IN_ARGS_SECTION");
		}

		unsigned int i = 0;
		TemplateDictionary *arg_dict;
		TemplateDictionary *adaptor_arg_dict;
		TemplateDictionary *all_args_dict;
                TemplateDictionary *inarg_dict_spec = nullptr;
		for (Xml::Nodes::iterator ai = args_in.begin(); ai != args_in.end(); ++ai, ++i)
		{
			Xml::Node &arg = **ai;
			string arg_type = signature_to_type(arg.get("type"));
			string arg_decl = "const " + arg_type + "& ";
			string arg_name = arg.get("name");
			arg_name = arg_name.empty() ?
					("argin" + make_string(i)) : legalize(arg_name);
			arg_decl += arg_name;
                        // goes through sync/async methods
			for (m = 0; m < 2; m++)
			{
				arg_dict = method_dicts[m]->AddSectionDictionary("METHOD_ARG_LIST");

				arg_dict->SetValue("METHOD_ARG_DECL", arg_decl);
				arg_dict->SetValue("METHOD_ARG_NAME", arg_name);

				TemplateDictionary *inarg_dict = method_dicts[m]->AddSectionDictionary("FOR_EACH_METHOD_IN_ARG");
				inarg_dict->SetValue("METHOD_IN_ARG_NAME", arg_name);
				inarg_dict->SetValue("METHOD_IN_ARG_TYPE", arg_type);
                                inarg_dict->SetValue("METHOD_IN_ARG_COMMA", ", ");
                                inarg_dict->SetIntValue("METHOD_IN_ARG_COUNT", i);
                                // We only care about the sync case as we will not be using async
                                // calls in the first iteration.
                                if (m == 0)
                                {
                                        inarg_dict_spec = inarg_dict;
                                }

				all_args_dict = method_dicts[m]->AddSectionDictionary("FOR_EACH_METHOD_ARG");
				all_args_dict->SetValue("METHOD_ARG_NAME", arg_name);
				all_args_dict->SetValue("METHOD_ARG_SIG", arg.get("type"));
				all_args_dict->SetValue("METHOD_ARG_IN_OUT", "true");
			}
			adaptor_arg_dict = sync_method_dict->AddSectionDictionary("METHOD_ADAPTOR_ARG_LIST");
			adaptor_arg_dict->SetValue("METHOD_ARG_DECL", arg_decl);
			adaptor_arg_dict->SetValue("METHOD_ARG_NAME", arg_name);
		}
                if (inarg_dict_spec)
                {
                        inarg_dict_spec->SetValue("METHOD_IN_ARG_COMMA", "");
                }
		arg_dict = async_method_dict->AddSectionDictionary("METHOD_ARG_LIST");
		arg_dict->SetValue("METHOD_ARG_DECL", "void* __data");
		arg_dict = async_method_dict->AddSectionDictionary("METHOD_ARG_LIST");
		arg_dict->SetValue("METHOD_ARG_DECL", "int __timeout=-1");

		// generate all 'out' arguments for a method signature
		if (args_out.size() > 0)
		{
			sync_method_dict->SetValue("METHOD_INVOKE_RET", "::DBus::Message __ret = ");
			for (m = 0; m < 2; m++)
				method_dicts[m]->ShowSection("METHOD_OUT_ARGS_SECTION");
		}

		TemplateDictionary *cbarg_dict;
		TemplateDictionary *outarg_list_dict;
		i = 0;
		if (args_out.size() == 1)
		{
			TemplateDictionary *outarg_dict
					= sync_method_dict->AddSectionDictionary("FOR_EACH_METHOD_OUT_ARG");
			Xml::Node *arg = args_out.front();
			sync_method_dict->ShowSection("METHOD_RETURN");
			sync_method_dict->SetValue("METHOD_RETURN_NAME", "__argout");
			sync_method_dict->SetValue("METHOD_RETURN_ASSIGN", "__argout = ");
			outarg_dict->SetValue("METHOD_OUT_ARG_NAME", "__argout");
			outarg_dict->SetValue("METHOD_OUT_ARG_TYPE",
					      signature_to_type(arg->get("type")));
			outarg_dict->ShowSection("METHOD_OUT_ARG_DECL");
			all_args_dict = sync_method_dict->AddSectionDictionary("FOR_EACH_METHOD_ARG");
			all_args_dict->SetValue("METHOD_ARG_NAME", arg->get("name"));
			all_args_dict->SetValue("METHOD_ARG_SIG", arg->get("type"));
			all_args_dict->SetValue("METHOD_ARG_IN_OUT", "false");
			TemplateDictionary *user_arg_dict =
					async_method_dict->AddSectionDictionary("FOR_EACH_METHOD_USER_OUT_ARG");
			string arg_name = arg->get("name");
			arg_name = arg_name.empty() ?  "__argout" : legalize(arg_name);
			user_arg_dict->SetValue("METHOD_OUT_ARG_NAME", arg_name);
			string arg_sig = signature_to_type(arg->get("type"));
			user_arg_dict->SetValue("METHOD_OUT_ARG_TYPE", arg_sig);
			outarg_list_dict = async_method_dict->AddSectionDictionary("METHOD_OUT_ARG_LIST");
			outarg_list_dict->SetValue("METHOD_OUT_ARG_NAME", arg_name);
			cbarg_dict = async_method_dict->AddSectionDictionary("METHOD_CALLBACK_ARG_LIST");
			string const_arg_decl = "const " + arg_sig + "& /*" + arg_name + "*/";
			cbarg_dict->SetValue("METHOD_CALLBACK_ARG", const_arg_decl);
		}
		else
		{
			for (Xml::Nodes::iterator ao = args_out.begin(); ao != args_out.end(); ++ao, ++i)
			{
				Xml::Node &arg = **ao;
				string arg_name = arg.get("name");
				arg_name = arg_name.empty() ?
						("__argout" + make_string(i)) : legalize(arg_name);
				string arg_sig = signature_to_type(arg.get("type"));
				string arg_decl = arg_sig + "& " + arg_name;
				for (m = 0; m < 2; m++)
				{
					TemplateDictionary *outarg_dict =
							method_dicts[m]->AddSectionDictionary("FOR_EACH_METHOD_OUT_ARG");

					outarg_dict->SetValue("METHOD_OUT_ARG_NAME", arg_name);
					outarg_dict->SetValue("METHOD_OUT_ARG_TYPE", arg_sig);

					TemplateDictionary *user_arg_dict =
							method_dicts[m]->AddSectionDictionary("FOR_EACH_METHOD_USER_OUT_ARG");
					user_arg_dict->SetValue("METHOD_OUT_ARG_NAME", arg_name);
					user_arg_dict->SetValue("METHOD_OUT_ARG_TYPE", arg_sig);
					outarg_list_dict = method_dicts[m]->AddSectionDictionary("METHOD_OUT_ARG_LIST");
					outarg_list_dict->SetValue("METHOD_OUT_ARG_NAME", arg_name);
					all_args_dict = method_dicts[m]->AddSectionDictionary("FOR_EACH_METHOD_ARG");
					all_args_dict->SetValue("METHOD_ARG_NAME", arg_name);
					all_args_dict->SetValue("METHOD_ARG_SIG", arg.get("type"));
					all_args_dict->SetValue("METHOD_ARG_IN_OUT", "false");
				}
				adaptor_arg_dict = sync_method_dict->AddSectionDictionary("METHOD_ADAPTOR_ARG_LIST");
				adaptor_arg_dict->SetValue("METHOD_ARG_DECL", arg_decl);
				adaptor_arg_dict->SetValue("METHOD_ARG_NAME", arg_name);
				sync_method_dict->AddSectionDictionary("METHOD_ARG_LIST")->SetValue("METHOD_ARG_DECL", arg_decl);
				cbarg_dict = async_method_dict->AddSectionDictionary("METHOD_CALLBACK_ARG_LIST");
				string const_arg_decl = "const " + arg_sig + "& /*" + arg_name + "*/";
				cbarg_dict->SetValue("METHOD_CALLBACK_ARG", const_arg_decl);
			}
		}
		adaptor_arg_dict = sync_method_dict->AddSectionDictionary("METHOD_ADAPTOR_ARG_LIST");
		adaptor_arg_dict->SetValue("METHOD_ARG_NAME", "__error");
		adaptor_arg_dict->SetValue("METHOD_ARG_DECL", "::DBus::Error &error");
		cbarg_dict = async_method_dict->AddSectionDictionary("METHOD_CALLBACK_ARG_LIST");
		cbarg_dict->SetValue("METHOD_CALLBACK_ARG", "const ::DBus::Error& /*__error*/, void* /*__data*/");
		outarg_list_dict = async_method_dict->AddSectionDictionary("METHOD_OUT_ARG_LIST");
		outarg_list_dict->SetValue("METHOD_OUT_ARG_NAME", "__error, __data");
	}
}

void generate_interfaces(Xml::Node &iface,
                         const std::string &instance_name,
                         TemplateDictionary &dict,
                         bool sync_mode,
                         bool async_mode)
{
        Xml::Nodes methods = iface["method"];
        Xml::Nodes signals = iface["signal"];
        Xml::Nodes properties = iface["property"];

        string ifacename = iface.get("name");
        
        TemplateDictionary *if_dict = dict.AddSectionDictionary("FOR_EACH_INTERFACE");

        if_dict->SetValue("INTERFACE_NAME", ifacename);
        if (sync_mode)
                if_dict->ShowSection("SYNC_SECTION");
        if (async_mode)
                if_dict->ShowSection("ASYNC_SECTION");

        istringstream ss(ifacename);
        string nspace;
        // generates all the namespaces defined with <interface name="X.Y.Z">
        while (ss.str().find('.', ss.tellg()) != string::npos)
        {
                getline(ss, nspace, '.');
                TemplateDictionary *ns_dict = if_dict->AddSectionDictionary("FOR_EACH_NAMESPACE");
                ns_dict->SetValue("NAMESPACE_NAME",  nspace);
        }

        if (instance_name != "")
        {
                TemplateDictionary *inst_dict = if_dict->AddSectionDictionary("HAS_INSTANCE");
                inst_dict->SetValue("INST_NAME", instance_name);
        }
        
        string ifaceclass;

        getline(ss, ifaceclass);

        if_dict->SetValue("CLASS_NAME", ifaceclass);

        // this loop generates all properties
        for (Xml::Nodes::iterator pi = properties.begin ();
             pi != properties.end (); ++pi)
        {
                Xml::Node &property = **pi;
                string prop_name = property.get ("name");
                string type = property.get("type");
                string property_access = property.get ("access");
                TemplateDictionary *prop_dict = if_dict->AddSectionDictionary("FOR_EACH_PROPERTY");

                prop_dict->SetValue("PROP_NAME", legalize(prop_name));
                prop_dict->SetValue("PROP_SIG", type);
                prop_dict->SetValue("PROP_TYPE", signature_to_type(type));

                if (!is_primitive_type(type))
                {
                        prop_dict->ShowSection("PROP_CONST");
                }

                if (!is_simple_type(type))
                {
                        prop_dict->ShowSection("HAS_COMPLEX_TYPE");

                        Ty ty = signature_to_ty(type);
                        if (ty.tplParams.size() == 2 && ty.ident == "std::map")
                        {
                                if (ty.tplParams[0].ident != "std::string")
                                {
                                        cerr << "Can't create property update fory type: " << ty << ", first template parameter must be string" << endl;
                                        exit(-1);
                                }
                                if (ty.tplParams[1].tplParams.size() != 0)
                                {
                                        cerr << "Can't create property update for type: " << ty << ", since second template parameter is not simple" << endl;
                                        exit(-1);
                                }
                                prop_dict->ShowSection("MAP_TYPE");
                        }
                        else if (ty.ident == "::DBus::Struct")
                        {
                                TemplateDictionary *type_dict = prop_dict->AddSectionDictionary("STRUCT_TYPE");
                                // To allow us to use _1 _2 etc when accessing the struct
                                int cnt = 1;
                                for (Ty& t : ty.tplParams)
                                {
                                        TemplateDictionary *arg_dict = type_dict->AddSectionDictionary("FOR_EACH_TYPE");
                                        arg_dict->SetValue("ARG_TYPE", t.ident);
                                        arg_dict->SetIntValue("ARG_COUNT", cnt);
                                        ++cnt;
                                }
                        }
                        else
                        {
                                cerr << "Can't create property update for type: " << ty << endl;
                                exit(-1);
                        }
                }
                else
                {
                        prop_dict->ShowSection("HAS_SIMPLE_TYPE");
                }

                if (property_access == "read" || property_access == "readwrite")
                {
                        prop_dict->ShowSection("PROPERTY_GETTER");
                        prop_dict->SetValue("PROP_READABLE", "true");
                }
                else
                {
                        prop_dict->SetValue("PROP_READABLE", "false");
                }
                if (property_access == "write" || property_access == "readwrite")
                {
                        prop_dict->ShowSection("PROPERTY_SETTER");
                        prop_dict->SetValue("PROP_WRITEABLE", "true");
                }
                else
                {
                        prop_dict->SetValue("PROP_WRITEABLE", "false");
                }
        }
        generate_methods(if_dict, methods);

        // this loop generates all signals
        for (Xml::Nodes::iterator si = signals.begin(); si != signals.end(); ++si)
        {
                Xml::Node &signal = **si;
                Xml::Nodes args = signal["arg"];

                TemplateDictionary *sig_dict = if_dict->AddSectionDictionary("FOR_EACH_SIGNAL");
                sig_dict->SetValue("SIGNAL_NAME", legalize(signal.get("name")));

                // this loop generates all arguments for a signal
                if (args.size() != 0)
                        sig_dict->ShowSection("SIGNAL_ARGS_SECTION");

                unsigned int i = 0;
                for (Xml::Nodes::iterator ai = args.begin(); ai != args.end(); ++ai, ++i)
                {
                        Xml::Node &arg = **ai;
                        TemplateDictionary *arg_dict = sig_dict->AddSectionDictionary("FOR_EACH_SIGNAL_ARG");
                        TemplateDictionary *arg_list_dict = sig_dict->AddSectionDictionary("SIGNAL_ARG_LIST");
                        TemplateDictionary *const_arg_dict = sig_dict->AddSectionDictionary("CONST_SIGNAL_ARG_LIST");

                        string arg_decl = signature_to_type(arg.get("type")) + " ";
                        string const_arg_decl = "const " + arg_decl + "&";
                        string arg_name = arg.get("name");
                        arg_name = arg_name.empty() ?
                                ("argin" + make_string(i)) : legalize(arg_name);
                        arg_decl += arg_name;
                        const_arg_decl += arg_name;
                        arg_dict->SetValue("SIGNAL_ARG_NAME", arg_name);
                        arg_dict->SetValue("SIGNAL_ARG_SIG", arg.get("type"));
                        arg_dict->SetValue("SIGNAL_ARG_DECL", arg_decl);
                        arg_list_dict->SetValue("SIGNAL_ARG_NAME", arg_name);
                        const_arg_dict->SetValue("SIGNAL_ARG_DECL", const_arg_decl);
                }
        }
}

void generate_stubs(Xml::Document &doc, const char *filename,
                    const std::vector< std::pair<string, string> > &macros,
		    bool sync_mode, bool async_mode,
                    const InterfaceInstances &iface_instances,
                    const char *template_file)
{
	TemplateDictionary dict("stubs-glue");
	string filestring = filename;
	underscorize(filestring);

	for(vector< pair<string, string> >::const_iterator iter = macros.begin();
	    iter != macros.end();
	    iter++) {
		dict.SetValue(iter->first, iter->second);
	}
	dict.SetValue("FILE_STRING", filestring);
        dict.SetValue("AUTO_GENERATED_WARNING",
                      "This file was automatically generated by dbusxx-xml2cpp;"
                      " DO NOT EDIT!");

	Xml::Node &root = *(doc.root);
	Xml::Nodes interfaces = root["interface"];

	// iterate over all interface definitions
	for (Xml::Nodes::iterator i = interfaces.begin(); i != interfaces.end(); ++i)
	{
		Xml::Node &iface = **i;

		// gets the name of an interface: <interface name="XYZ">
		string ifacename = iface.get("name");

		// these interface names are skipped.
		if (ifacename == "org.freedesktop.DBus.Introspectable")
		{
			cerr << "skipping interface " << ifacename << endl;
			continue;
		}

                // lookup the interface instances that we want to create
                std::vector<std::string> instances{};
                for (auto const& iface : iface_instances)
                {
                        if (iface.first == ifacename)
                        {
                                instances = iface.second;
                                break;
                        }
                }

                if (instances.size() == 0)
                {
                        generate_interfaces(iface, "", dict, sync_mode, async_mode);
                }
                for (auto const& inst_name : instances)
                {
                        generate_interfaces(iface, inst_name, dict, sync_mode, async_mode);
                }
	}

	ofstream file(filename);
	if (file.bad())
	{
		cerr << "unable to write file " << filename << endl;
		exit(-1);
	}

	string output;
	ExpandTemplate(template_file, STRIP_BLANK_LINES, &dict, &output);
	file << output;
	file.close();
}
