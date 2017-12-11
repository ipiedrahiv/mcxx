/*--------------------------------------------------------------------
  (C) Copyright 2006-2013 Barcelona Supercomputing Center
                          Centro Nacional de Supercomputacion

  This file is part of Mercurium C/C++ source-to-source compiler.

  See AUTHORS file in the top level directory for information
  regarding developers and contributors.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 3 of the License, or (at your option) any later version.

  Mercurium C/C++ source-to-source compiler is distributed in the hope
  that it will be useful, but WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the GNU Lesser General Public License for more
  details.

  You should have received a copy of the GNU Lesser General Public
  License along with Mercurium C/C++ source-to-source compiler; if
  not, write to the Free Software Foundation, Inc., 675 Mass Ave,
  Cambridge, MA 02139, USA.
--------------------------------------------------------------------*/

#include "tl-counters.hpp"
#include "tl-lowering-visitor.hpp"
#include "tl-lowering-utils.hpp"
#include "tl-lower-task-common.hpp"
#include "tl-symbol-utils.hpp"

namespace TL { namespace Intel {

void create_task_function(const Nodecl::OpenMP::Task& construct,
                          const TL::Scope& scope,
                          TL::Symbol& outline_task,
                          Nodecl::NodeclBase& outline_task_code,
                          Nodecl::NodeclBase& outline_task_stmt) {

    TL::Type kmp_int32_type = scope
        .get_symbol_from_name("kmp_int32")
        .get_user_defined_type();
    ERROR_CONDITION(!kmp_int32_type.is_valid(), "Type kmp_int32 not in scope", 0);
    TL::Type kmp_task_type = scope
                            .get_symbol_from_name("kmp_task_t")
                            .get_user_defined_type();
    ERROR_CONDITION(!kmp_task_type.is_valid(), "Type kmp_task_type not in scope", 0);

    TL::Symbol enclosing_function = Nodecl::Utils::get_enclosing_function(construct);

    TL::Counter &task_num = TL::CounterManager::get_counter("intel-omp-task");
    std::stringstream outline_task_name;
    outline_task_name << "_task_" << enclosing_function.get_name() << "_" << (int)task_num;
    task_num++;

    TL::ObjectList<std::string> parameter_names;
    TL::ObjectList<TL::Type> parameter_types;

    parameter_names.append("_global_tid"); parameter_types.append(kmp_int32_type);
    parameter_names.append("_task"); parameter_types.append(kmp_task_type.get_pointer_to());

    outline_task = SymbolUtils::new_function_symbol(
            enclosing_function,
            outline_task_name.str(),
            TL::Type::get_void_type(),
            parameter_names,
            parameter_types);

    SymbolUtils::build_empty_body_for_function(outline_task,
            outline_task_code,
            outline_task_stmt);
}

void create_task_args(const Nodecl::OpenMP::Task& construct,
                      const TL::Scope& scope,
                      const TL::Symbol& outline_task,
                      const TL::ObjectList<TL::Symbol>& shared_no_vla_symbols,
                      const TL::ObjectList<TL::Symbol>& shared_vla_symbols,
                      const TL::ObjectList<TL::Symbol>& firstprivate_no_vla_symbols,
                      const TL::ObjectList<TL::Symbol>& firstprivate_vla_symbols,
                      TL::Type& task_args_type) {

    std::stringstream task_args_struct_name;
    task_args_struct_name << "_args" << outline_task.get_name();

    Source src_task_args_struct;
    src_task_args_struct << "struct " << task_args_struct_name.str() << " {";
    for (TL::ObjectList<TL::Symbol>::const_iterator it = firstprivate_no_vla_symbols.begin();
            it != firstprivate_no_vla_symbols.end();
            it++)
    {
        src_task_args_struct
            << as_type(it->get_type().no_ref().get_unqualified_type())
            << " "
            << it->get_name()
            << ";";
    }

    for (TL::ObjectList<TL::Symbol>::const_iterator it = firstprivate_vla_symbols.begin();
            it != firstprivate_vla_symbols.end();
            it++)
    {
        src_task_args_struct
            << as_type(TL::Type::get_void_type().get_pointer_to())
            << " "
            << it->get_name()
            << ";";
    }

    for (TL::ObjectList<TL::Symbol>::const_iterator it = shared_no_vla_symbols.begin();
            it != shared_no_vla_symbols.end();
            it++)
    {
        src_task_args_struct
            << as_type(it->get_type().no_ref().get_pointer_to())
            << " "
            << it->get_name()
            << ";";
    }

    for (TL::ObjectList<TL::Symbol>::const_iterator it = shared_vla_symbols.begin();
            it != shared_vla_symbols.end();
            it++)
    {
        src_task_args_struct
            << as_type(TL::Type::get_void_type().get_pointer_to())
            << " "
            << it->get_name()
            << ";";
    }

    src_task_args_struct << "};";
    Nodecl::NodeclBase tree_task_args_struct = src_task_args_struct.parse_declaration(scope);

    if (IS_CXX_LANGUAGE)
    {
        Nodecl::Utils::prepend_to_enclosing_top_level_location(construct, tree_task_args_struct);
    }

    std::string task_args_struct_lang_name = task_args_struct_name.str();
    if (IS_C_LANGUAGE)
        task_args_struct_lang_name  = "struct " + task_args_struct_lang_name;

    
    task_args_type = scope
        .get_symbol_from_name(task_args_struct_lang_name)
        .get_user_defined_type();

}

void create_src_task_final(const TaskEnvironmentVisitor& task_environment,
                           TL::Source& src_task_final) {

    if (!task_environment.final_condition.is_null()) {
        src_task_final << "(" << as_expression(task_environment.final_condition) << "? 2 : 0" << ")";
    }
    else {
        src_task_final << "0";
    }

}

void create_src_task_untied(const TaskEnvironmentVisitor& task_environment,
                            TL::Source& src_task_untied) {

    src_task_untied
    << !task_environment.is_untied;
}

bool is_vla(const Type& t) {
    if (t.no_ref().is_pointer()) {
        return is_vla(t.no_ref().points_to());
    }
    else if (t.no_ref().is_array() && t.no_ref().array_is_vla()) {
        return true;
    }
    else {
        return false;
    }
}

void split_symbol_list_in_vla_notvla(const TL::ObjectList<TL::Symbol>& symbols,
                                 TL::ObjectList<TL::Symbol>& no_vla_symbols,
                                 TL::ObjectList<TL::Symbol>& vla_symbols) {
    for (auto it = symbols.begin(); it != symbols.end(); it++) {
       if (is_vla(it->get_type())) {
           vla_symbols.insert(*it);
       }
       else {
           no_vla_symbols.insert(*it);
       }
    }
}

void capture_vars(const TL::Type& task_args_type,
                  const TL::ObjectList<TL::Symbol>& firstprivate_no_vla_symbols,
                  const TL::ObjectList<TL::Symbol>& firstprivate_vla_symbols,
                  const TL::ObjectList<TL::Symbol>& shared_no_vla_symbols,
                  const TL::ObjectList<TL::Symbol>& shared_vla_symbols,
                  Nodecl::NodeclBase& stmt_task_fill) {

    Source src_task_fill;
    src_task_fill
    << "_args = (" << as_type(task_args_type) << "*)" << "_ret->shareds;";
    Nodecl::NodeclBase tree_task_fill = src_task_fill.parse_statement(stmt_task_fill);
    stmt_task_fill.prepend_sibling(tree_task_fill);

    TL::ObjectList<TL::Symbol> fields = task_args_type.get_fields();
    TL::ObjectList<TL::Symbol>::iterator it_fields = fields.begin();
    for (TL::ObjectList<TL::Symbol>::const_iterator it = firstprivate_no_vla_symbols.begin();
		    it != firstprivate_no_vla_symbols.end();
		    it++, it_fields++)
    {
        Source src_task_args_capture;
        if (!it->get_type().no_ref().is_array()) {
	    src_task_args_capture
		    << "_args" << "->" << it_fields->get_name() << " = " << it->get_name() << ";";
        }
        else {
            src_task_args_capture
            << "__builtin_memcpy(_args->" << it_fields->get_name() << ","
            <<                        it->get_name()
            <<                        ", sizeof(" << as_symbol(*it_fields) << "));";
        }
        Nodecl::NodeclBase tree_task_args_capture = src_task_args_capture.parse_statement(stmt_task_fill);
        stmt_task_fill.prepend_sibling(tree_task_args_capture);
    }

    Source src_args_vla_fp_points_to;
    src_args_vla_fp_points_to
    << "(char *)(_args + 1)";
    for (TL::ObjectList<TL::Symbol>::const_iterator it = firstprivate_vla_symbols.begin();
		    it != firstprivate_vla_symbols.end();
		    it++, it_fields++)
    {
        Source src_task_args_capture;
        src_task_args_capture
        << "_args" << "->" << it_fields->get_name() << " = " << src_args_vla_fp_points_to << ";"
        << "__builtin_memcpy(_args->" << it_fields->get_name() << ","
        <<                        it->get_name()
        <<                        ", sizeof(" << as_symbol(*it) << "));";

        Nodecl::NodeclBase tree_task_args_capture = src_task_args_capture.parse_statement(stmt_task_fill);
        stmt_task_fill.prepend_sibling(tree_task_args_capture);

        src_args_vla_fp_points_to
        << " + sizeof(" << it->get_name() << ")";
    }

    for (TL::ObjectList<TL::Symbol>::const_iterator it = shared_no_vla_symbols.begin();
		    it != shared_no_vla_symbols.end();
		    it++, it_fields++)
    {
        Source src_task_args_capture;
        src_task_args_capture
        << "_args" << "->" << it_fields->get_name() << " = &" << it->get_name() << ";";
        Nodecl::NodeclBase tree_task_args_capture = src_task_args_capture.parse_statement(stmt_task_fill);
        stmt_task_fill.prepend_sibling(tree_task_args_capture);
    }

    for (TL::ObjectList<TL::Symbol>::const_iterator it = shared_vla_symbols.begin();
		    it != shared_vla_symbols.end();
		    it++, it_fields++)
    {
        Source src_task_args_capture;
        src_task_args_capture
        << "_args" << "->" << it_fields->get_name() << " = &" << it->get_name() << ";";
        Nodecl::NodeclBase tree_task_args_capture = src_task_args_capture.parse_statement(stmt_task_fill);
        stmt_task_fill.prepend_sibling(tree_task_args_capture);
    }
}

void task_vars_definition(const TL::Type& task_args_type,
                          const TL::ObjectList<TL::Symbol>& firstprivate_no_vla_symbols,
                          const TL::ObjectList<TL::Symbol>& firstprivate_vla_symbols,
                          const TL::ObjectList<TL::Symbol>& shared_no_vla_symbols,
                          const TL::ObjectList<TL::Symbol>& shared_vla_symbols,
                          const TL::ObjectList<TL::Symbol>& private_no_vla_symbols,
                          const TL::ObjectList<TL::Symbol>& private_vla_symbols,
                          Nodecl::NodeclBase& outline_task_stmt) {

    Source src_task_prev;
    src_task_prev
    << as_type(task_args_type) << " *_args = (" << as_type(task_args_type) << "*)" << "_task->shareds;";
    Nodecl::NodeclBase tree_task_prev = src_task_prev.parse_statement(outline_task_stmt);
    outline_task_stmt.prepend_sibling(tree_task_prev);

    TL::ObjectList<TL::Symbol> fields = task_args_type.get_fields();
    TL::ObjectList<TL::Symbol>::iterator it_fields = fields.begin();
    for (TL::ObjectList<TL::Symbol>::const_iterator it = firstprivate_no_vla_symbols.begin();
		    it != firstprivate_no_vla_symbols.end();
		    it++, it_fields++)
    {
        Source src_task_var_definition;
        src_task_var_definition
        << as_type(it->get_type().no_ref().get_unqualified_type().get_lvalue_reference_to())
        << " _task_"
        << it->get_name()
        << " = "
        << "(*_args)."
        << it_fields->get_name()
        << ";";
        Nodecl::NodeclBase tree_task_var_definition = src_task_var_definition.parse_statement(outline_task_stmt);
        outline_task_stmt.prepend_sibling(tree_task_var_definition);
    }

    for (TL::ObjectList<TL::Symbol>::const_iterator it = firstprivate_vla_symbols.begin();
		    it != firstprivate_vla_symbols.end();
		    it++, it_fields++)
    {
        Nodecl::Utils::SimpleSymbolMap vla_symbol_map;
        TL::ObjectList<TL::Symbol> vla_symbols;
        Intel::gather_vla_symbols(*it, vla_symbols);

        for (auto vla_it = vla_symbols.begin(); vla_it != vla_symbols.end(); vla_it++) {
            TL::Symbol new_symbol = outline_task_stmt
                                    .retrieve_context()
                                    .get_symbol_from_name("_task_" + vla_it->get_name());
            vla_symbol_map.add_map(*vla_it, new_symbol);
        }

        TL::Type new_type = ::type_deep_copy(it->get_type().get_internal_type(),
                                it->get_scope().get_decl_context(),
                                vla_symbol_map.get_symbol_map());

        Source src_task_var_definition;
        src_task_var_definition
        << as_type(new_type.get_lvalue_reference_to())
        << " _task_"
        << it->get_name()
        << " = "
        << "*(" << as_type(new_type) << "*)((*_args)."
        << it_fields->get_name()
        << ");";
        Nodecl::NodeclBase tree_task_var_definition = src_task_var_definition.parse_statement(outline_task_stmt);
        outline_task_stmt.prepend_sibling(tree_task_var_definition);
    }

    for (TL::ObjectList<TL::Symbol>::const_iterator it = shared_no_vla_symbols.begin();
		    it != shared_no_vla_symbols.end();
		    it++, it_fields++)
    {
        Source src_task_var_definition;
        src_task_var_definition
        << as_type(it->get_type().no_ref().get_lvalue_reference_to())
        << " _task_"
        << it->get_name()
        << " = "
        << "*(*_args)."
        << it_fields->get_name()
        << ";";
        Nodecl::NodeclBase tree_task_var_definition = src_task_var_definition.parse_statement(outline_task_stmt);
        outline_task_stmt.prepend_sibling(tree_task_var_definition);
    }

    for (TL::ObjectList<TL::Symbol>::const_iterator it = shared_vla_symbols.begin();
		    it != shared_vla_symbols.end();
		    it++, it_fields++)
    {
        Nodecl::Utils::SimpleSymbolMap vla_symbol_map;
        TL::ObjectList<TL::Symbol> vla_symbols;
        Intel::gather_vla_symbols(*it, vla_symbols);

        for (auto vla_it = vla_symbols.begin(); vla_it != vla_symbols.end(); vla_it++) {
            TL::Symbol new_symbol = outline_task_stmt
                                    .retrieve_context()
                                    .get_symbol_from_name("_task_" + vla_it->get_name());
            vla_symbol_map.add_map(*vla_it, new_symbol);
        }

        TL::Type new_type = ::type_deep_copy(it->get_type().get_internal_type(),
                                it->get_scope().get_decl_context(),
                                vla_symbol_map.get_symbol_map());

        Source src_task_var_definition;
        src_task_var_definition
        << as_type(new_type.get_lvalue_reference_to())
        << " _task_"
        << it->get_name()
        << " = "
        << "*(" << as_type(new_type) << "*)((*_args)."
        << it_fields->get_name()
        << ");";
        Nodecl::NodeclBase tree_task_var_definition = src_task_var_definition.parse_statement(outline_task_stmt);
        outline_task_stmt.prepend_sibling(tree_task_var_definition);
    }

    for (TL::ObjectList<TL::Symbol>::const_iterator it = private_no_vla_symbols.begin();
		    it != private_no_vla_symbols.end();
		    it++)
    {
        Source src_task_var_definition;
        src_task_var_definition
        << as_type(it->get_type())
        << " _task_"
        << it->get_name()
        << ";";
        Nodecl::NodeclBase tree_task_var_definition = src_task_var_definition.parse_statement(outline_task_stmt);
        outline_task_stmt.prepend_sibling(tree_task_var_definition);
    }

    for (TL::ObjectList<TL::Symbol>::const_iterator it = private_vla_symbols.begin();
		    it != private_vla_symbols.end();
		    it++)
    {
        Nodecl::Utils::SimpleSymbolMap vla_symbol_map;
        TL::ObjectList<TL::Symbol> vla_symbols;
        Intel::gather_vla_symbols(*it, vla_symbols);

        for (auto vla_it = vla_symbols.begin(); vla_it != vla_symbols.end(); vla_it++) {
            TL::Symbol new_symbol = outline_task_stmt
                                    .retrieve_context()
                                    .get_symbol_from_name("_task_" + vla_it->get_name());
            vla_symbol_map.add_map(*vla_it, new_symbol);
        }

        TL::Type new_type = ::type_deep_copy(it->get_type().get_internal_type(),
                                it->get_scope().get_decl_context(),
                                vla_symbol_map.get_symbol_map());

        Source src_task_var_definition;
        src_task_var_definition
        << as_type(new_type)
        << " _task_"
        << it->get_name()
        << ";";
        Nodecl::NodeclBase tree_task_var_definition = src_task_var_definition.parse_statement(outline_task_stmt);
        outline_task_stmt.prepend_sibling(tree_task_var_definition);
    }
}

void fill_symbol_map(Nodecl::Utils::SimpleSymbolMap& symbol_map,
                     const TL::ObjectList<TL::Symbol>& firstprivate_no_vla_symbols,
                     const TL::ObjectList<TL::Symbol>& firstprivate_vla_symbols,
                     const TL::ObjectList<TL::Symbol>& shared_no_vla_symbols,
                     const TL::ObjectList<TL::Symbol>& shared_vla_symbols,
                     const TL::ObjectList<TL::Symbol>& private_no_vla_symbols,
                     const TL::ObjectList<TL::Symbol>& private_vla_symbols,
                     const Nodecl::NodeclBase& outline_task_stmt) {

    TL::ObjectList<TL::Symbol> all_symbols;

    all_symbols.insert(shared_no_vla_symbols);
    all_symbols.insert(shared_vla_symbols);
    all_symbols.insert(private_no_vla_symbols);
    all_symbols.insert(private_vla_symbols);
    all_symbols.insert(firstprivate_no_vla_symbols);
    all_symbols.insert(firstprivate_vla_symbols);

    for (TL::ObjectList<TL::Symbol>::const_iterator it = all_symbols.begin();
            it != all_symbols.end();
            it++)
    {
        // retrieve_context busca por los nodos de arriba el scope
        TL::Symbol task_sym = outline_task_stmt.retrieve_context().get_symbol_from_name("_task_" + it->get_name());
        ERROR_CONDITION(!task_sym.is_valid(), "Invalid symbol", 0);
        symbol_map.add_map(*it, task_sym);
    }
}

void LoweringVisitor::visit(const Nodecl::OpenMP::Task& construct)
{
    Nodecl::NodeclBase statements = construct.get_statements();
    Nodecl::List environment = construct.get_environment().as<Nodecl::List>();

    walk(statements);
    statements = construct.get_statements(); // Should not be necessary

    TaskEnvironmentVisitor task_environment;
    task_environment.walk(environment);

    TL::ObjectList<Nodecl::OpenMP::Shared> shared_list = environment.find_all<Nodecl::OpenMP::Shared>();
    TL::ObjectList<Nodecl::OpenMP::Private> private_list = environment.find_all<Nodecl::OpenMP::Private>();
    TL::ObjectList<Nodecl::OpenMP::Firstprivate> firstprivate_list = environment.find_all<Nodecl::OpenMP::Firstprivate>();
    TL::ObjectList<Nodecl::OpenMP::DepIn> depin_list = environment.find_all<Nodecl::OpenMP::DepIn>();
    TL::ObjectList<Nodecl::OpenMP::DepOut> depout_list = environment.find_all<Nodecl::OpenMP::DepOut>();
    TL::ObjectList<Nodecl::OpenMP::DepInout> depinout_list = environment.find_all<Nodecl::OpenMP::DepInout>();
    TL::ObjectList<Nodecl::OpenMP::Reduction> reduction_list = environment.find_all<Nodecl::OpenMP::Reduction>();

    TL::ObjectList<TL::Symbol> private_symbols;
    TL::ObjectList<TL::Symbol> private_no_vla_symbols;
    TL::ObjectList<TL::Symbol> private_vla_symbols;

    TL::ObjectList<TL::Symbol> firstprivate_symbols;
    TL::ObjectList<TL::Symbol> firstprivate_no_vla_symbols;
    TL::ObjectList<TL::Symbol> firstprivate_vla_symbols;

    TL::ObjectList<TL::Symbol> shared_no_vla_symbols;
    TL::ObjectList<TL::Symbol> shared_vla_symbols;
    TL::ObjectList<TL::Symbol> shared_symbols;

    TL::ObjectList<Nodecl::NodeclBase> depin_nodecl;
    TL::ObjectList<Nodecl::NodeclBase> depout_nodecl;
    TL::ObjectList<Nodecl::NodeclBase> depinout_nodecl;

    if (!shared_list.empty())
    {
        TL::ObjectList<Symbol> tmp =
            shared_list  // TL::ObjectList<OpenMP::Shared>
            .map<Nodecl::NodeclBase>(&Nodecl::OpenMP::Shared::get_symbols) // TL::ObjectList<Nodecl::NodeclBase>
            .map<Nodecl::List>(&Nodecl::NodeclBase::as<Nodecl::List>) // TL::ObjectList<Nodecl::List>
            .map<TL::ObjectList<Nodecl::NodeclBase> >(&Nodecl::List::to_object_list) // TL::ObjectList<TL::ObjectList<Nodecl::NodeclBase> >
            .reduction(TL::append_two_lists<Nodecl::NodeclBase>) // TL::ObjectList<Nodecl::NodeclBase>
            .map<TL::Symbol>(&Nodecl::NodeclBase::get_symbol) // TL::ObjectList<TL::Symbol>
            ;

        shared_symbols.insert(tmp);
        split_symbol_list_in_vla_notvla(shared_symbols, shared_no_vla_symbols, shared_vla_symbols);
    }
    if (!private_list.empty())
    {
        TL::ObjectList<Symbol> tmp =
            private_list  // TL::ObjectList<OpenMP::Private>
            .map<Nodecl::NodeclBase>(&Nodecl::OpenMP::Private::get_symbols) // TL::ObjectList<Nodecl::NodeclBase>
            .map<Nodecl::List>(&Nodecl::NodeclBase::as<Nodecl::List>) // TL::ObjectList<Nodecl::List>
            .map<TL::ObjectList<Nodecl::NodeclBase> >(&Nodecl::List::to_object_list) // TL::ObjectList<TL::ObjectList<Nodecl::NodeclBase> >
            .reduction(TL::append_two_lists<Nodecl::NodeclBase>) // TL::ObjectList<Nodecl::NodeclBase>
            .map<TL::Symbol>(&Nodecl::NodeclBase::get_symbol) // TL::ObjectList<TL::Symbol>
            ;

        private_symbols.insert(tmp);
        split_symbol_list_in_vla_notvla(private_symbols, private_no_vla_symbols, private_vla_symbols);
    }
    if (!firstprivate_list.empty())
    {
        TL::ObjectList<Symbol> tmp =
            firstprivate_list  // TL::ObjectList<OpenMP::Firstprivate>
            .map<Nodecl::NodeclBase>(&Nodecl::OpenMP::Firstprivate::get_symbols) // TL::ObjectList<Nodecl::NodeclBase>
            .map<Nodecl::List>(&Nodecl::NodeclBase::as<Nodecl::List>) // TL::ObjectList<Nodecl::List>
            .map<TL::ObjectList<Nodecl::NodeclBase> >(&Nodecl::List::to_object_list) // TL::ObjectList<TL::ObjectList<Nodecl::NodeclBase> >
            .reduction(TL::append_two_lists<Nodecl::NodeclBase>) // TL::ObjectList<Nodecl::NodeclBase>
            .map<TL::Symbol>(&Nodecl::NodeclBase::get_symbol) // TL::ObjectList<TL::Symbol>
            ;

        firstprivate_symbols.insert(tmp);
        split_symbol_list_in_vla_notvla(firstprivate_symbols, firstprivate_no_vla_symbols, firstprivate_vla_symbols);
    }
    bool no_deps = true;
    if (!depin_list.empty())
    {
        no_deps = false;
        TL::ObjectList<Nodecl::NodeclBase> tmp =
            depin_list  // TL::ObjectList<OpenMP::DepIn>
            .map<Nodecl::NodeclBase>(&Nodecl::OpenMP::DepIn::get_exprs) // TL::ObjectList<Nodecl::NodeclBase>
            .map<Nodecl::List>(&Nodecl::NodeclBase::as<Nodecl::List>) // TL::ObjectList<Nodecl::List>
            .map<TL::ObjectList<Nodecl::NodeclBase> >(&Nodecl::List::to_object_list) // TL::ObjectList<TL::ObjectList<Nodecl::NodeclBase> >
            .reduction(TL::append_two_lists<Nodecl::NodeclBase>) // TL::ObjectList<Nodecl::NodeclBase>
            ;

        depin_nodecl.insert(tmp);
    }
    if (!depout_list.empty())
    {
        no_deps = false;
        TL::ObjectList<Nodecl::NodeclBase> tmp =
            depout_list  // TL::ObjectList<OpenMP::DepOut>
            .map<Nodecl::NodeclBase>(&Nodecl::OpenMP::DepOut::get_exprs) // TL::ObjectList<Nodecl::NodeclBase>
            .map<Nodecl::List>(&Nodecl::NodeclBase::as<Nodecl::List>) // TL::ObjectList<Nodecl::List>
            .map<TL::ObjectList<Nodecl::NodeclBase> >(&Nodecl::List::to_object_list) // TL::ObjectList<TL::ObjectList<Nodecl::NodeclBase> >
            .reduction(TL::append_two_lists<Nodecl::NodeclBase>) // TL::ObjectList<Nodecl::NodeclBase>
            ;

        depout_nodecl.insert(tmp);
    }
    if (!depinout_list.empty())
    {
        no_deps = false;
        TL::ObjectList<Nodecl::NodeclBase> tmp =
            depinout_list  // TL::ObjectList<OpenMP::DepInout>
            .map<Nodecl::NodeclBase>(&Nodecl::OpenMP::DepInout::get_exprs) // TL::ObjectList<Nodecl::NodeclBase>
            .map<Nodecl::List>(&Nodecl::NodeclBase::as<Nodecl::List>) // TL::ObjectList<Nodecl::List>
            .map<TL::ObjectList<Nodecl::NodeclBase> >(&Nodecl::List::to_object_list) // TL::ObjectList<TL::ObjectList<Nodecl::NodeclBase> >
            .reduction(TL::append_two_lists<Nodecl::NodeclBase>) // TL::ObjectList<Nodecl::NodeclBase>
            ;

        depinout_nodecl.insert(tmp);
    }

    // END VARS

    TL::Scope global_scope = CURRENT_COMPILED_FILE->global_decl_context;

    TL::Type kmp_task_type = global_scope
                            .get_symbol_from_name("kmp_task_t")
                            .get_user_defined_type();

    TL::Type kmp_depend_info_type = global_scope
                            .get_symbol_from_name("kmp_depend_info_t")
                            .get_user_defined_type();

    TL::Type kmp_routine_type = global_scope
                                .get_symbol_from_name("kmp_routine_entry_t")
                                .get_user_defined_type();

    TL::Symbol ident_symbol = Intel::new_global_ident_symbol(construct);

    TL::Symbol outline_task;
    Nodecl::NodeclBase outline_task_code, outline_task_stmt;
    create_task_function(construct,
                         global_scope,
                         outline_task,
                         outline_task_code,
                         outline_task_stmt);

    TL::Type task_args_type;
    create_task_args(construct,
                     global_scope,
                     outline_task,
                     shared_no_vla_symbols,
                     shared_vla_symbols,
                     firstprivate_no_vla_symbols,
                     firstprivate_vla_symbols,
                     task_args_type);


    // Poner el codigo de crear task, estructuras...
    Source src_task_call_body;
    Nodecl::NodeclBase stmt_definitions, stmt_task_alloc, stmt_task_fill, stmt_task;
    Nodecl::NodeclBase stmt_set_priority;
    src_task_call_body
    << "{"
    << statement_placeholder(stmt_definitions)
    << statement_placeholder(stmt_task_alloc)
    << statement_placeholder(stmt_set_priority)
    << statement_placeholder(stmt_task_fill)
    << statement_placeholder(stmt_task)
    << "}";
    Nodecl::NodeclBase tree_task_call_body = src_task_call_body.parse_statement(construct);

    Source src_definitions;
    src_definitions
    << as_type(kmp_task_type) << " *_ret;"
    << as_type(task_args_type) << " *_args;";
    Nodecl::NodeclBase tree_definitions = src_definitions.parse_statement(stmt_definitions);
    // Por que no puedo hacer replace en lugar de prepend_sibling?
    stmt_definitions.prepend_sibling(tree_definitions);

    Source src_task_final;
    Source src_task_untied;
    create_src_task_final(task_environment, src_task_final);
    create_src_task_untied(task_environment, src_task_untied);

    Source sizeof_args_expr;
    sizeof_args_expr
    << "sizeof(" << as_type(task_args_type) << ")";

    for (auto it = firstprivate_vla_symbols.begin(); it != firstprivate_vla_symbols.end(); it++) {
        sizeof_args_expr << " + sizeof(" << as_symbol(*it) << ")";
    }


    Source src_task_alloc;
    src_task_alloc
    << "_ret = __kmpc_omp_task_alloc(&" << as_symbol(ident_symbol) << ","
                                 << "__kmpc_global_thread_num(&" << as_symbol(ident_symbol) << "),"
                                 << src_task_final << "|" << src_task_untied << ","
                                 << "sizeof(" << as_type(kmp_task_type) << "),"
                                 << sizeof_args_expr << ","
                                 << "(" << as_type(kmp_routine_type) << ")&" << as_symbol(outline_task) << ");";

    Nodecl::NodeclBase tree_task_alloc = src_task_alloc.parse_statement(stmt_task_alloc);
    stmt_task_alloc.replace(tree_task_alloc);

    if (!task_environment.priority.is_null()) {
        Source src_set_priority;
        src_set_priority
        << "_ret->data2.priority = " << as_expression(task_environment.priority) << ";";

        Nodecl::NodeclBase tree_set_priority = src_set_priority.parse_statement(stmt_set_priority);
        stmt_set_priority.replace(tree_set_priority);
    }

    capture_vars(task_args_type,
                 firstprivate_no_vla_symbols,
                 firstprivate_vla_symbols,
                 shared_no_vla_symbols,
                 shared_vla_symbols,
                 stmt_task_fill);

    task_vars_definition(task_args_type,
                         firstprivate_no_vla_symbols,
                         firstprivate_vla_symbols,
                         shared_no_vla_symbols,
                         shared_vla_symbols,
                         private_no_vla_symbols,
                         private_vla_symbols,
                         outline_task_stmt);

    Nodecl::Utils::SimpleSymbolMap symbol_map;
    fill_symbol_map(symbol_map,
                    firstprivate_no_vla_symbols,
                    firstprivate_vla_symbols,
                    shared_no_vla_symbols,
                    shared_vla_symbols,
                    private_no_vla_symbols,
                    private_vla_symbols,
                    outline_task_stmt);

    Nodecl::NodeclBase task_func_body = Nodecl::Utils::deep_copy(statements,
            outline_task_stmt,
            symbol_map);
    outline_task_stmt.prepend_sibling(task_func_body);

    Nodecl::Utils::prepend_to_enclosing_top_level_location(construct, outline_task_code);

    Source src_task_call;
    Source src_task_if_with_deps;
    Source src_task_deps_def;
    if (no_deps) {
        src_task_call
        << "__kmpc_omp_task(&" << as_symbol(ident_symbol) << ","
        << "__kmpc_global_thread_num(&" << as_symbol(ident_symbol) << "),"
        << "(" << as_type(kmp_task_type) << " *)_ret);";
    }
    else {
            src_task_deps_def
            << as_type(kmp_depend_info_type)
            << " _deps["
            << depin_nodecl.size() + depout_nodecl.size() + depinout_nodecl.size()
            << "];";

            int array_pos = 0;

            for (auto it = depin_nodecl.begin(); it != depin_nodecl.end(); it++, array_pos++) {
                DataReference data_ref(*it);
                Nodecl::NodeclBase address_of_object = data_ref.get_base_address();
                Nodecl::NodeclBase offset_of_object = data_ref.get_offsetof_dependence();
                Nodecl::NodeclBase sizeof_object = data_ref.get_sizeof();
                src_task_deps_def
                << "_deps[" << array_pos << "].base_addr = (kmp_intptr_t)((kmp_uint8 *)" << as_expression(address_of_object) << " + " << as_expression(offset_of_object) << ");"
                << "_deps[" << array_pos << "].len = " << as_expression(sizeof_object) << ";"
                << "_deps[" << array_pos << "].flags.in = 1;";
            }
            for (auto it = depout_nodecl.begin(); it != depout_nodecl.end(); it++, array_pos++) {
                DataReference data_ref(*it);
                Nodecl::NodeclBase address_of_object = data_ref.get_base_address();
                Nodecl::NodeclBase offset_of_object = data_ref.get_offsetof_dependence();
                Nodecl::NodeclBase sizeof_object = data_ref.get_sizeof();
                src_task_deps_def
                << "_deps[" << array_pos << "].base_addr = (kmp_intptr_t)((kmp_uint8 *)" << as_expression(address_of_object) << " + " << as_expression(offset_of_object) << ");"
                << "_deps[" << array_pos << "].len = " << as_expression(sizeof_object) << ";"
                << "_deps[" << array_pos << "].flags.in = 1;" // FIXME: it should be zero, but Intel's Runtime requires that
                << "_deps[" << array_pos << "].flags.out = 1;";
            }
            for (auto it = depinout_nodecl.begin(); it != depinout_nodecl.end(); it++, array_pos++) {
                DataReference data_ref(*it);
                Nodecl::NodeclBase address_of_object = data_ref.get_base_address();
                Nodecl::NodeclBase offset_of_object = data_ref.get_offsetof_dependence();
                Nodecl::NodeclBase sizeof_object = data_ref.get_sizeof();
                src_task_deps_def
                << "_deps[" << array_pos << "].base_addr = (kmp_intptr_t)((kmp_uint8 *)" << as_expression(address_of_object) << " + " << as_expression(offset_of_object) << ");"
                << "_deps[" << array_pos << "].len = " << as_expression(sizeof_object) << ";"
                << "_deps[" << array_pos << "].flags.in = 1;"
                << "_deps[" << array_pos << "].flags.out = 1;";
            }
            src_task_call
            << "__kmpc_omp_task_with_deps(&" << as_symbol(ident_symbol) << ","
            << "__kmpc_global_thread_num(&" << as_symbol(ident_symbol) << "),"
            << "(" << as_type(kmp_task_type) << " *)_ret," << array_pos << ", _deps, 0, 0);";

            // This is used only when task has depenencies and an if clause
            src_task_if_with_deps
            << "__kmpc_omp_wait_deps(&" << as_symbol(ident_symbol) << ","
            << "__kmpc_global_thread_num(&" << as_symbol(ident_symbol) << "),"
            << array_pos << ", _deps, 0, 0);";
    }

    Source src_task_if;
    if (!task_environment.if_condition.is_null()) {
        src_task_if
        << "if (" << as_expression(task_environment.if_condition) << ") {"
            << src_task_call
        << "} else {"
            << src_task_if_with_deps
            << "__kmpc_omp_task_begin_if0(&" << as_symbol(ident_symbol) << ","
            << "__kmpc_global_thread_num(&" << as_symbol(ident_symbol) << "),"
            << "(" << as_type(kmp_task_type) << " *)_ret);"
            << as_symbol(outline_task) << "("
            << "__kmpc_global_thread_num(&" << as_symbol(ident_symbol) << "),"
            << "(" << as_type(kmp_task_type) << " *)_ret);"
            << "__kmpc_omp_task_complete_if0(&" << as_symbol(ident_symbol) << ","
            << "__kmpc_global_thread_num(&" << as_symbol(ident_symbol) << "),"
            << "(" << as_type(kmp_task_type) << " *)_ret);"
        << "}";
    }
    else {
        src_task_if
        << src_task_call;
    }
    Source src_task;
    src_task
    << "{"
    << src_task_deps_def
    << src_task_if
    << "}";
    Nodecl::NodeclBase tree_task = src_task.parse_statement(stmt_task);
    stmt_task.replace(tree_task);

    construct.replace(tree_task_call_body);
}

} }
