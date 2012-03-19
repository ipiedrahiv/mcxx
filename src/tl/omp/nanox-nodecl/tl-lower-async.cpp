/*--------------------------------------------------------------------
  (C) Copyright 2006-2012 Barcelona Supercomputing Center
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

#include "tl-lowering-visitor.hpp"
#include "tl-nanos.hpp"
#include "tl-source.hpp"
#include "tl-counters.hpp"
#include "tl-nodecl-alg.hpp"
#include "tl-datareference.hpp"

#include "cxx-cexpr.h"

using TL::Source;

namespace TL { namespace Nanox {

struct TaskEnvironmentVisitor : public Nodecl::ExhaustiveVisitor<void>
{
    public:
        bool is_untied;
        Nodecl::NodeclBase priority;

        TaskEnvironmentVisitor()
            : is_untied(false),
            priority()
        {
        }

        void visit(const Nodecl::Parallel::Priority& priority)
        {
            this->priority = priority.get_priority();
        }

        void visit(const Nodecl::Parallel::Untied& untied)
        {
            this->is_untied = true;
        }
};

static TL::Symbol declare_const_wd_type(int num_devices)
{
    static std::map<int, Symbol> _map;
    std::map<int, Symbol>::iterator it = _map.find(num_devices);

    if (it == _map.end())
    {
        std::stringstream ss;
        if (IS_C_LANGUAGE)
        {
            ss << "struct ";
        }
        ss << "nanos_const_wd_definition_" << num_devices;

        TL::Scope sc(CURRENT_COMPILED_FILE->global_decl_context);
        TL::Symbol new_class_symbol = sc.new_symbol(ss.str());
        new_class_symbol.get_internal_symbol()->kind = SK_CLASS;
        new_class_symbol.get_internal_symbol()->entity_specs.is_user_declared = 1;

        type_t* new_class_type = get_new_class_type(sc.get_decl_context(), TT_STRUCT);
        decl_context_t class_context = new_class_context(sc.get_decl_context(), new_class_symbol.get_internal_symbol());
        TL::Scope class_scope(class_context);

        class_type_set_inner_context(new_class_type, class_context);

        new_class_symbol.get_internal_symbol()->type_information = new_class_type;

        _map[num_devices] = new_class_symbol;

        {
            // Base field
            TL::Symbol base_class = sc.get_symbol_from_name("nanos_const_wd_definition_t");
            ERROR_CONDITION(!base_class.is_valid(), "Invalid symbol", 0);

            TL::Symbol field = class_scope.new_symbol("base");
            field.get_internal_symbol()->kind = SK_VARIABLE;
            field.get_internal_symbol()->entity_specs.is_user_declared = 1;

            field.get_internal_symbol()->entity_specs.is_member = 1;
            field.get_internal_symbol()->entity_specs.class_type = ::get_user_defined_type(new_class_symbol.get_internal_symbol());
            field.get_internal_symbol()->entity_specs.access = AS_PUBLIC;

            field.get_internal_symbol()->file = "";
            field.get_internal_symbol()->line = 0;

            field.get_internal_symbol()->type_information = ::get_user_defined_type(base_class.get_internal_symbol());
            class_type_add_member(new_class_type, field.get_internal_symbol());
        }

        {
            // Devices field
            TL::Symbol devices_class = sc.get_symbol_from_name("nanos_device_t");
            ERROR_CONDITION(!devices_class.is_valid(), "Invalid symbol", 0);

            TL::Symbol field = class_scope.new_symbol("devices");
            field.get_internal_symbol()->kind = SK_VARIABLE;
            field.get_internal_symbol()->entity_specs.is_user_declared = 1;

            field.get_internal_symbol()->entity_specs.is_member = 1;
            field.get_internal_symbol()->entity_specs.class_type = ::get_user_defined_type(new_class_symbol.get_internal_symbol());


            field.get_internal_symbol()->entity_specs.access = AS_PUBLIC;

            field.get_internal_symbol()->file = "";
            field.get_internal_symbol()->line = 0;

            field.get_internal_symbol()->type_information = 
                ::get_array_type(
                        ::get_user_defined_type(devices_class.get_internal_symbol()),
                        const_value_to_nodecl( const_value_get_signed_int(num_devices)),
                        class_scope.get_decl_context());

            class_type_add_member(new_class_type, field.get_internal_symbol());
        }

        return new_class_symbol;
    }
    else
    {
        return it->second;
    }
}

void LoweringVisitor::emit_async_common(
        Nodecl::NodeclBase construct,
        TL::Symbol function_symbol, 
        Nodecl::NodeclBase statements,
        Nodecl::NodeclBase priority_expr,
        bool is_untied,

        OutlineInfo& outline_info)
{
    Source spawn_code;

    Source struct_arg_type_name,
           struct_runtime_size,
           struct_size,
           copy_decl,
           copy_data,
           immediate_decl,
           copy_imm_data,
           translation_fun_arg_name,
           const_wd_info;

    Nodecl::NodeclBase fill_outline_arguments_tree,
        fill_dependences_outline_tree;
    Source fill_outline_arguments,
           fill_dependences_outline;

    Nodecl::NodeclBase fill_immediate_arguments_tree,
        fill_dependences_immediate_tree;
    Source fill_immediate_arguments,
           fill_dependences_immediate;
    
    std::string outline_name;
    {
        Counter& task_counter = CounterManager::get_counter("nanos++-outline");
        std::stringstream ss;
        ss << "ol_" << function_symbol.get_name() << "_" << (int)task_counter;
        outline_name = ss.str();

        task_counter++;
    }


    // Declare argument structure
    TL::Symbol structure_symbol = declare_argument_structure(outline_info, construct);
    struct_arg_type_name << structure_symbol.get_name();

    {
        // Static stuff
        //
        // typedef struct { 
        //     nanos_wd_props_t props; 
        //     size_t data_alignment; 
        //     size_t num_copies; 
        //     size_t num_devices; 
        // } nanos_const_wd_definition_t; 

        // FIXME
        int num_devices = 1;
        TL::Symbol const_wd_type = declare_const_wd_type(num_devices);

        Source alignment, props_init, num_copies;

        Source device_descriptor, 
               device_description, 
               device_description_init, 
               num_devices_src,
               ancillary_device_description,
               fortran_dynamic_init;

        const_wd_info
            << device_description
            << "static " << const_wd_type.get_name() << " nanos_wd_const_data = {"
            << "{"
            << /* ".props = " << */ props_init << ", \n"
            << /* ".data_alignment = " << */ alignment << ", \n"
            << /* ".num_copies = " << */ num_copies << ",\n"
            << /* ".num_devices = " << */ num_devices_src << ",\n"
            << "}, "
            << /* ".devices = " << */ "{" << device_description_init << "}"
            << "};"

            << fortran_dynamic_init

            << "nanos_wd_dyn_props_t nanos_wd_dyn_props;"
            << "nanos_wd_dyn_props.tie_to = 0;"
            ;

        alignment << "__alignof__(" << struct_arg_type_name << ")";
        num_copies << "0";

        device_descriptor << outline_name << "_devices";
        device_description
            << ancillary_device_description
            ;

        Source mandatory_creation,
               tiedness,
               priority;

        // We expand all the struct due to a limitation in the FE. See ticket
        // #963
        props_init
            << "{ " 
            << /* ".mandatory_creation = " << */ mandatory_creation << ",\n"
            << /* ".tied = " << */ tiedness << ",\n"
            << /* ".reserved0 =" << */ "0,\n"
            << /* ".reserved1 =" << */ "0,\n"
            << /* ".reserved2 =" << */ "0,\n"
            << /* ".reserved3 =" << */ "0,\n"
            << /* ".reserved4 =" << */ "0,\n"
            << /* ".reserved5 =" << */ "0,\n"
            << /* ".priority = " << */ priority 
            << "}"
            ;

        mandatory_creation << "0";
        tiedness << (int)!is_untied;

        if (priority_expr.is_null())
        {
            priority << "0";
        }
        else
        {
            priority << as_expression(priority_expr);
        }

        // FIXME - No devices yet, let's mimick the structure of one SMP
        {
            num_devices_src << num_devices;

            if (!IS_FORTRAN_LANGUAGE)
            {
                ancillary_device_description
                    << "static nanos_smp_args_t " << outline_name << "_smp_args = {" 
                    << ".outline = (void(*)(void*))&" << outline_name 
                    << "};"
                    ;
                device_description_init
                    << "{"
                    << /* factory */ "&nanos_smp_factory, &" << outline_name << "_smp_args"
                    << "}"
                    ;
            }
            else
            {
                // We'll fill them later because of Fortran limitations
                ancillary_device_description
                    << "static nanos_smp_args_t " << outline_name << "_smp_args;" 
                    ;
                device_description_init
                    << "{"
                    // factory, arg
                    << "0, 0"
                    << "}"
                    ;
                fortran_dynamic_init
                    << outline_name << "_smp_args.outline = (void(*)(void*))&" << outline_name << ";"
                    << "nanos_wd_const_data.devices[0].factory = &nanos_smp_factory;"
                    << "nanos_wd_const_data.devices[0].arg = &" << outline_name << "_smp_args;"
                    ;
            }
        }
    }

    Source dynamic_size;
    struct_size << "sizeof(imm_args)" << dynamic_size;

    translation_fun_arg_name << "(void (*)(void*, void*))0";
    copy_data << "(nanos_copy_data_t**)0";
    copy_imm_data << "(nanos_copy_data_t*)0";

    // Outline
    emit_outline(outline_info, statements, outline_name, structure_symbol);

    Source err_name;
    err_name << "err";

    // Account for the extra size due to overallocated items
    bool there_are_overallocated = false;
    bool immediate_is_alloca = false;

    // We overallocate with an alignment of 8
    const int overallocation_alignment = 8;
    const int overallocation_mask = overallocation_alignment - 1;

    TL::ObjectList<OutlineDataItem> data_items = outline_info.get_data_items();
    if (IS_C_LANGUAGE
            || IS_CXX_LANGUAGE)
    {
        for (TL::ObjectList<OutlineDataItem>::iterator it = data_items.begin();
                it != data_items.end();
                it++)
        {
            if ((it->get_allocation_policy() & OutlineDataItem::ALLOCATION_POLICY_OVERALLOCATED) 
                    == OutlineDataItem::ALLOCATION_POLICY_OVERALLOCATED)
            {
                dynamic_size << "+ " << overallocation_alignment << " + sizeof(" << it->get_symbol().get_name() << ")";
                there_are_overallocated = true;
            }
        }
    }

    if (there_are_overallocated)
    {
        immediate_is_alloca = true;
    }
    
    // Fill argument structure
    if (!immediate_is_alloca)
    {
        immediate_decl
            << struct_arg_type_name << " imm_args;"
            ;
    }
    else
    {
        immediate_decl
            // Create a rebindable reference
            << struct_arg_type_name << "@reb-ref@ imm_args;"
            // Note that in rebindable referencex "&x" is a "T* lvalue" (not a "T* rvalue" as usual)
            << "&imm_args = (" << struct_arg_type_name << " *) __builtin_alloca(" << struct_size << ");"
            ;
    }

    Source num_dependences;

    // Spawn code
    spawn_code
        << "{"
        // Devices related to this task
        <<     const_wd_info
        <<     struct_arg_type_name << "* ol_args;"
        <<     "ol_args = (" << struct_arg_type_name << "*) 0;"
        <<     immediate_decl
        <<     struct_runtime_size
        <<     "nanos_wd_t wd = (nanos_wd_t)0;"
        <<     "nanos_wd_props_t props;"
        <<     copy_decl
        <<     "nanos_err_t " << err_name <<";"
        // nanos_err_t nanos_create_wd_compact ( nanos_wd_t *wd, nanos_const_wd_definition_t *const_data, size_t data_size,
        //                               void ** data, nanos_wg_t wg, nanos_copy_data_t **copies )
        <<     err_name << " = nanos_create_wd_compact(&wd, &(nanos_wd_const_data.base), &nanos_wd_dyn_props, " 
        <<                 struct_size << ", (void**)&ol_args, nanos_current_wd(),"
        <<                 copy_data << ");"
        <<     "if (" << err_name << " != NANOS_OK) nanos_handle_error (" << err_name << ");"
        <<     "if (wd != (nanos_wd_t)0)"
        <<     "{"
        <<        statement_placeholder(fill_outline_arguments_tree)
        <<        fill_dependences_outline
        <<        err_name << " = nanos_submit(wd, " << num_dependences << ", dependences, (nanos_team_t)0);"
        <<        "if (" << err_name << " != NANOS_OK) nanos_handle_error (" << err_name << ");"
        <<     "}"
        <<     "else"
        <<     "{"
        <<          statement_placeholder(fill_immediate_arguments_tree)
        <<          fill_dependences_immediate
        // nanos_err_t nanos_create_wd_and_run_compact ( nanos_const_wd_definition_t *const_data, size_t data_size, void * data, size_t num_deps, 
        //                                       nanos_dependence_t *deps, nanos_copy_data_t *copies, nanos_translate_args_t translate_args );
        <<          err_name << " = nanos_create_wd_and_run_compact(&(nanos_wd_const_data.base), &nanos_wd_dyn_props, "
        <<                  struct_size << ", " 
        <<                  "&imm_args,"
        <<                  num_dependences << ", dependences, "
        <<                  copy_imm_data << ", "
        <<                  translation_fun_arg_name << ");"
        <<          "if (" << err_name << " != NANOS_OK) nanos_handle_error (" << err_name << ");"
        <<     "}"
        << "}"
        ;

    // Fill arguments
    fill_arguments(construct, outline_info, fill_outline_arguments, fill_immediate_arguments);
    
    // Fill dependences for outline
    num_dependences << count_dependences(outline_info);

    fill_dependences(construct, 
            outline_info, 
            /* accessor */ Source("ol_args->"),
            fill_dependences_outline);
    fill_dependences(construct, 
            outline_info, 
            /* accessor */ Source("imm_args."),
            fill_dependences_immediate);

    FORTRAN_LANGUAGE()
    {
        // Parse in C
        Source::source_language = SourceLanguage::C;
    }
    Nodecl::NodeclBase spawn_code_tree = spawn_code.parse_statement(construct);

    FORTRAN_LANGUAGE()
    {
        Source::source_language = SourceLanguage::Current;
    }

    if (!fill_outline_arguments.empty())
    {
        Nodecl::NodeclBase new_tree = fill_outline_arguments.parse_statement(fill_outline_arguments_tree);
        fill_outline_arguments_tree.integrate(new_tree);
    }

    if (!fill_immediate_arguments.empty())
    {
        Nodecl::NodeclBase new_tree = fill_immediate_arguments.parse_statement(fill_immediate_arguments_tree);
        fill_immediate_arguments_tree.integrate(new_tree);
    }

    construct.integrate(spawn_code_tree);
}

void LoweringVisitor::visit(const Nodecl::Parallel::Async& construct)
{
    Nodecl::NodeclBase environment = construct.get_environment();
    Nodecl::NodeclBase statements = construct.get_statements();

    TaskEnvironmentVisitor task_environment;
    task_environment.walk(environment);

    OutlineInfo outline_info(environment);

    Symbol function_symbol = Nodecl::Utils::get_enclosing_function(construct);

    emit_async_common(
            construct,
            function_symbol, 
            statements, 
            task_environment.priority, 
            task_environment.is_untied, 

            outline_info);
}


void LoweringVisitor::fill_arguments(
        Nodecl::NodeclBase ctr,
        OutlineInfo& outline_info,
        // out
        Source& fill_outline_arguments,
        Source& fill_immediate_arguments
        )
{
    // We overallocate with an alignment of 8
    const int overallocation_alignment = 8;
    const int overallocation_mask = overallocation_alignment - 1;
    Source intptr_type;
    intptr_type << Type(::get_size_t_type()).get_declaration(ctr.retrieve_context(), "")
        ;

    TL::ObjectList<OutlineDataItem> data_items = outline_info.get_data_items();
    // Now fill the arguments information (this part is language dependent)
    if (IS_C_LANGUAGE
            || IS_CXX_LANGUAGE)
    {
        Source overallocation_base_offset; 
        // We round up to the alignment
        overallocation_base_offset << "(void*)(((" 
            << intptr_type << ")(char*)(ol_args + 1) + " 
            << overallocation_mask << ") & (~" << overallocation_mask << "))";

        Source imm_overallocation_base_offset;
        imm_overallocation_base_offset << "(void*)(((" 
            << intptr_type << ")(char*)(&imm_args + 1) + " 
            << overallocation_mask << ") & (~" << overallocation_mask << "))";

        for (TL::ObjectList<OutlineDataItem>::iterator it = data_items.begin();
                it != data_items.end();
                it++)
        {
            switch (it->get_sharing())
            {
                case OutlineDataItem::SHARING_CAPTURE:
                    {
                        if ((it->get_allocation_policy() & OutlineDataItem::ALLOCATION_POLICY_OVERALLOCATED)
                                == OutlineDataItem::ALLOCATION_POLICY_OVERALLOCATED)
                        {
                            TL::Type sym_type = it->get_symbol().get_type();
                            if (sym_type.is_any_reference())
                                sym_type = sym_type.references_to();

                            ERROR_CONDITION(!sym_type.is_array(), "Only arrays can be overallocated", 0);

                            // Overallocated
                            fill_outline_arguments << 
                                "ol_args->" << it->get_field_name() << " = " << Source(overallocation_base_offset) << ";"
                                ;

                            // Overwrite source
                            overallocation_base_offset = Source() << "(void*)((" 
                                << intptr_type << ")((char*)(ol_args->" << 
                                it->get_field_name() << ") + sizeof(" << it->get_symbol().get_name() << ") + " 
                                << overallocation_mask << ") & (~" << overallocation_mask << "))"
                                ;
                            fill_immediate_arguments << 
                                "imm_args." << it->get_field_name() << " = " << Source(imm_overallocation_base_offset) << ";";
                            ;
                            // Overwrite source
                            imm_overallocation_base_offset = Source() << "(void*)((" 
                                << intptr_type << ")((char*)(imm_args." << 
                                it->get_field_name() << ") + sizeof(" << it->get_symbol().get_name() << ") + "
                                << overallocation_mask << ") & (~" << overallocation_mask << "))"
                                ;

                            fill_outline_arguments
                                << "__builtin_memcpy(&ol_args->" << it->get_field_name() 
                                << ", &" << it->get_symbol().get_name() 
                                << ", sizeof(" << it->get_symbol().get_name() << "));"
                                ;
                            fill_immediate_arguments
                                << "__builtin_memcpy(&imm_args." << it->get_field_name() 
                                << ", &" << it->get_symbol().get_name() 
                                << ", sizeof(" << it->get_symbol().get_name() << "));"
                                ;
                        }
                        else
                        {
                            // Not overallocated
                            TL::Type sym_type = it->get_symbol().get_type();
                            if (sym_type.is_any_reference())
                                sym_type = sym_type.references_to();

                            if (sym_type.is_array())
                            {
                                fill_outline_arguments
                                    << "__builtin_memcpy(&ol_args->" << it->get_field_name() 
                                    << ", &" << it->get_symbol().get_name() 
                                    << ", sizeof(" << it->get_symbol().get_name() << "));"
                                    ;
                                fill_immediate_arguments
                                    << "__builtin_memcpy(&imm_args." << it->get_field_name() 
                                    << ", &" << it->get_symbol().get_name() 
                                    << ", sizeof(" << it->get_symbol().get_name() << "));"
                                    ;
                            }
                            else
                            {
                                // Plain assignment is enough
                                fill_outline_arguments << 
                                    "ol_args->" << it->get_field_name() << " = " << it->get_symbol().get_name() << ";"
                                    ;
                                fill_immediate_arguments << 
                                    "imm_args." << it->get_field_name() << " = " << it->get_symbol().get_name() << ";"
                                    ;
                            }
                        }
                        break;
                    }
                case  OutlineDataItem::SHARING_SHARED:
                    {
                        fill_outline_arguments << 
                            "ol_args->" << it->get_field_name() << " = &" << it->get_symbol().get_name() << ";"
                            ;
                        fill_immediate_arguments << 
                            "imm_args." << it->get_field_name() << " = &" << it->get_symbol().get_name() << ";"
                            ;
                        break;
                    }
                case  OutlineDataItem::SHARING_CAPTURE_ADDRESS:
                    {
                        Type t = it->get_shared_expression().get_type();

                        fill_outline_arguments 
                            << "ol_args->" << it->get_field_name() << " = " << as_expression( it->get_shared_expression().copy() ) << ";"
                            ;
                        fill_immediate_arguments 
                            << "imm_args." << it->get_field_name() << " = " << as_expression( it->get_shared_expression().copy() ) << ";"
                            ;
                        break;
                    }
                case OutlineDataItem::SHARING_PRIVATE:
                    {
                        // Do nothing
                        break;
                    }
                default:
                    {
                        internal_error("Unexpected sharing kind", 0);
                    }
            }
        }
    }
    else if (IS_FORTRAN_LANGUAGE)
    {
        for (TL::ObjectList<OutlineDataItem>::iterator it = data_items.begin();
                it != data_items.end();
                it++)
        {
            switch (it->get_sharing())
            {

                case OutlineDataItem::SHARING_CAPTURE:
                    {
                        fill_outline_arguments << 
                            "ol_args % " << it->get_field_name() << " = " << it->get_symbol().get_name() << "\n"
                            ;
                        fill_immediate_arguments << 
                            "imm_args % " << it->get_field_name() << " = " << it->get_symbol().get_name() << "\n"
                            ;
                        break;
                    }
                case OutlineDataItem::SHARING_SHARED:
                    {
                        fill_outline_arguments << 
                            "ol_args %" << it->get_field_name() << " => " << it->get_symbol().get_name() << "\n"
                            ;
                        fill_immediate_arguments << 
                            "imm_args % " << it->get_field_name() << " => " << it->get_symbol().get_name() << "\n"
                            ;
                        // Make it TARGET as required by Fortran
                        it->get_symbol().get_internal_symbol()->entity_specs.is_target = 1;
                        break;
                    }
                case OutlineDataItem::SHARING_CAPTURE_ADDRESS:
                    {
                        fill_outline_arguments << 
                            "ol_args %" << it->get_field_name() << " => " << as_expression( it->get_shared_expression().copy()) << "\n"
                            ;
                        fill_immediate_arguments << 
                            "imm_args % " << it->get_field_name() << " => " << as_expression( it->get_shared_expression().copy() ) << "\n"
                            ;
                        
                        // Best effort, this may fail sometimes
                        DataReference data_ref(it->get_shared_expression());
                        if (data_ref.is_valid())
                        {
                            // Make it TARGET as required by Fortran
                            data_ref.get_base_symbol().get_internal_symbol()->entity_specs.is_target = 1;
                        }
                        else
                        {
                            std::cerr 
                                << it->get_shared_expression().get_locus() 
                                << ": warning: an argument is not a valid data-reference, compilation is likely to fail" 
                                << std::endl;
                        }
                        break;
                    }
                case OutlineDataItem::SHARING_PRIVATE:
                    {
                        // Do nothing
                        break;
                    }
                default:
                    {
                        internal_error("Unexpected sharing kind", 0);
                    }
            }
        }
    }
}

int LoweringVisitor::count_dependences(OutlineInfo& outline_info)
{
    int num_deps = 0;

    TL::ObjectList<OutlineDataItem> data_items = outline_info.get_data_items();
    for (TL::ObjectList<OutlineDataItem>::iterator it = data_items.begin();
            it != data_items.end();
            it++)
    {
        if (it->get_directionality() == OutlineDataItem::DIRECTIONALITY_NONE)
            continue;

        num_deps += it->get_dependences().size();
    }

    return num_deps;
}

static void fill_dimensions(int n_dims, 
        int actual_dim, 
        int current_dep_num,
        Nodecl::NodeclBase * dim_sizes, 
        Type dep_type, 
        Source& dims_description, 
        Source& dependency_regions_code, 
        Scope sc);

void LoweringVisitor::fill_dependences(
        Nodecl::NodeclBase ctr,
        OutlineInfo& outline_info,
        Source arguments_accessor,
        // out
        Source& result_src
        )
{
    Source dependency_init;

    int num_deps = count_dependences(outline_info);

    TL::ObjectList<OutlineDataItem> data_items = outline_info.get_data_items();

    if (num_deps == 0)
    {
        if (Nanos::Version::interface_is_at_least("master", 6001))
        {
            result_src << "nanos_data_access_t dependences[1];"
                ;
        }
        else
        {
            result_src << "nanos_dependence_t dependences[1];"
                ;
        }

        return;
    }

    if (Nanos::Version::interface_is_at_least("master", 6001))
    {
        Source dependency_regions;

        result_src
            << dependency_regions
            << "nanos_data_access_t dependences[" << num_deps << "]"
            ; 
        
        if (IS_C_LANGUAGE
                || IS_CXX_LANGUAGE)
        {
            result_src << " = {"
                << dependency_init
                << "};"
                ;
        }
        result_src << ";"
            ;

        int current_dep_num = 0;
        for (TL::ObjectList<OutlineDataItem>::iterator it = data_items.begin();
                it != data_items.end();
                it++)
        {
            OutlineDataItem::Directionality dir = it->get_directionality();
            if (dir == OutlineDataItem::DIRECTIONALITY_NONE)
                continue;

            TL::ObjectList<Nodecl::NodeclBase> deps = it->get_dependences();
            for (ObjectList<Nodecl::NodeclBase>::iterator dep_it = deps.begin();
                    dep_it != deps.end();
                    dep_it++, current_dep_num++)
            {
                TL::DataReference dep_expr(*dep_it);

                Source dependency_offset,
                       dependency_flags,
                       dependency_flags_in,
                       dependency_flags_out,
                       dependency_flags_concurrent,
                       dependency_size;

                Source dep_expr_addr;
                dep_expr_addr << as_expression(dep_expr.get_base_address());
                dependency_size << as_expression(dep_expr.get_sizeof());

                dependency_flags 
                    << "{" 
                    << dependency_flags_in << "," 
                    << dependency_flags_out << ", "
                    << /* renaming has not yet been implemented */ "0, " 
                    << dependency_flags_concurrent
                    << "}"
                    ;

                Type dependency_type = dep_expr.get_data_type();

                int num_dimensions = dependency_type.get_num_dimensions();

                int concurrent = ((dir & OutlineDataItem::DIRECTIONALITY_CONCURRENT) == OutlineDataItem::DIRECTIONALITY_CONCURRENT);

                dependency_flags_in << (((dir & OutlineDataItem::DIRECTIONALITY_IN) == OutlineDataItem::DIRECTIONALITY_IN) || concurrent);
                dependency_flags_out << (((dir & OutlineDataItem::DIRECTIONALITY_OUT) == OutlineDataItem::DIRECTIONALITY_OUT) || concurrent);
                dependency_flags_concurrent << concurrent;
                //
                // Compute the base type of the dependency and the array containing the size of each dimension
                Type dependency_base_type = dependency_type;

                Nodecl::NodeclBase dimension_sizes[num_dimensions];
                for (int dim = 0; dim < num_dimensions; dim++)
                {
                    dimension_sizes[dim] = dependency_base_type.array_get_size();
                    dependency_base_type = dependency_base_type.array_element();
                }

                std::string base_type_name = dependency_base_type.get_declaration(dep_expr.retrieve_context(), "");

                dependency_regions << "nanos_region_dimension_t dimensions_" << current_dep_num << "[" << std::max(num_dimensions, 1) << "]"
                    ;

                Source dims_description;

                if (IS_C_LANGUAGE
                        || IS_CXX_LANGUAGE)
                {
                    dependency_regions << "=  { " << dims_description << "}";
                }

                dependency_regions << ";"
                    ;

                if (num_dimensions == 0)
                {
                    Source dimension_size, dimension_lower_bound, dimension_accessed_length;

                    dimension_size << as_expression(dimension_sizes[num_dimensions - 1].copy()) << "* sizeof(" << base_type_name << ")";
                    dimension_lower_bound << "0";
                    dimension_accessed_length << dimension_size;

                    if (IS_C_LANGUAGE
                            || IS_CXX_LANGUAGE)
                    {
                        Source dims_description;
                        dims_description
                            << "{"
                            << dimension_size << ","
                            << dimension_lower_bound << ","
                            << dimension_accessed_length
                            << "}"
                            ;
                    }
                    else
                    {
                        dependency_regions
                            << "dimensions_" << current_dep_num << "[0].size = " << dimension_size << ";"
                            << "dimensions_" << current_dep_num << "[0].lower_bound = " << dimension_lower_bound << ";"
                            << "dimensions_" << current_dep_num << "[0].accessed_length = " << dimension_accessed_length << ";"
                            ;
                    }
                }
                else
                {
                    Source dimension_size, dimension_lower_bound, dimension_accessed_length;

                    // Compute the contiguous array type
                    Type contiguous_array_type = dependency_type;
                    while (contiguous_array_type.array_element().is_array())
                    {
                        contiguous_array_type = contiguous_array_type.array_element();
                    }

                    Nodecl::NodeclBase lb, ub, size;
                    if (contiguous_array_type.array_is_region())
                    {
                        contiguous_array_type.array_get_region_bounds(lb, ub);
                        size = contiguous_array_type.array_get_region_size();
                    }
                    else
                    {
                        contiguous_array_type.array_get_bounds(lb, ub);
                        size = contiguous_array_type.array_get_size();
                    }

                    dimension_size << "sizeof(" << base_type_name << ") * " << as_expression(dimension_sizes[num_dimensions - 1].copy());
                    dimension_lower_bound << "sizeof(" << base_type_name << ") * " << as_expression(lb.copy());
                    dimension_accessed_length << "sizeof(" << base_type_name << ") * " << as_expression(size.copy());

                    if (IS_C_LANGUAGE
                            || IS_CXX_LANGUAGE)
                    {
                        Source dims_description;
                        dims_description
                            << "{"
                            << dimension_size << ","
                            << dimension_lower_bound << ","
                            << dimension_accessed_length
                            << "}"
                            ;
                    }
                    else
                    {
                        dependency_regions
                            << "dimensions_" << current_dep_num << "[0].size = " << dimension_size << ";"
                            << "dimensions_" << current_dep_num << "[0].lower_bound = " << dimension_lower_bound << ";"
                            << "dimensions_" << current_dep_num << "[0].accessed_length = " << dimension_accessed_length << ";"
                            ;
                    }
                    
                    // All but 0 (contiguous) are handled here
                    fill_dimensions(num_dimensions, 
                            num_dimensions,
                            current_dep_num,
                            dimension_sizes, 
                            dependency_type, 
                            dims_description,
                            dependency_regions,
                            dep_expr.retrieve_context());
                }

                if (num_dimensions == 0) 
                    num_dimensions++;

                if (IS_C_LANGUAGE
                        || IS_CXX_LANGUAGE)
                {
                    dependency_init
                        << "{"
                        << "(void*)" << arguments_accessor << it->get_field_name() << ","
                        << dependency_flags << ", "
                        << num_dimensions << ", "
                        << "dimensions_" << current_dep_num
                        << "}";
                }
                else if (IS_FORTRAN_LANGUAGE)
                {
                    result_src
                        << "dependences[" << current_dep_num << "].address = (void*)" << arguments_accessor << it->get_field_name() << ";"
                        << "dependences[" << current_dep_num << "].flags.input = " << dependency_flags_in << ";"
                        << "dependences[" << current_dep_num << "].flags.output" << dependency_flags_out << ";"
                        << "dependences[" << current_dep_num << "].flags.can_rename = 0;"
                        << "dependences[" << current_dep_num << "].flags.commutative = " << dependency_flags_concurrent << ";"
                        << "dependences[" << current_dep_num << "].dimension_count = " << num_dimensions << ";"
                        << "dependences[" << current_dep_num << "].dimensions = dimensions_" << current_dep_num << ";"
                        ;
                }
            }
        }
    }
    else
    {
        result_src
            << "nanos_dependence_t dependences[" << num_deps << "]";

        // We only initialize in C/C++, in Fortran we will make a set of assignments
        if (IS_C_LANGUAGE
                || IS_CXX_LANGUAGE)
        {
            result_src << "= {"
            << dependency_init
            << "}";
        }

        result_src << ";"
            ;

        int current_dep_num = 0;
        for (TL::ObjectList<OutlineDataItem>::iterator it = data_items.begin();
                it != data_items.end();
                it++)
        {
            OutlineDataItem::Directionality dir = it->get_directionality();
            if (dir == OutlineDataItem::DIRECTIONALITY_NONE)
                continue;

            TL::ObjectList<Nodecl::NodeclBase> deps = it->get_dependences();
            for (ObjectList<Nodecl::NodeclBase>::iterator dep_it = deps.begin();
                    dep_it != deps.end();
                    dep_it++, current_dep_num++)
            {
                TL::DataReference dep_expr(*dep_it);

                Source current_dependency_init,
                       dependency_offset,
                       dependency_flags,
                       dependency_flags_in,
                       dependency_flags_out,
                       dependency_flags_concurrent,
                       dependency_size;

                if (IS_C_LANGUAGE
                        || IS_CXX_LANGUAGE)
                {
                    current_dependency_init
                        << "{"
                        << "(void**)&(" << arguments_accessor << it->get_field_name() << "),"
                        << dependency_offset << ","
                        << dependency_flags << ","
                        << dependency_size
                        << "}"
                        ;
                }
                else if (IS_FORTRAN_LANGUAGE)
                {
                    // We use plain assignments in Fortran
                    result_src
                        << "dependences[" << current_dep_num << "].address = " << "(void**)&(" << arguments_accessor << it->get_field_name() << ");"
                        << "dependences[" << current_dep_num << "].offset = " << dependency_offset << ";"
                        << "dependences[" << current_dep_num << "].size = " << dependency_size << ";"
                        << "dependences[" << current_dep_num << "].flags.input = " << dependency_flags_in << ";"
                        << "dependences[" << current_dep_num << "].flags.output = " << dependency_flags_out << ";"
                        << "dependences[" << current_dep_num << "].flags.can_rename = 0;"
                        ;
                    
                    // if (Nanos::Version::interface_is_at_least("master", 5001))
                    {
                        result_src
                            << "dependences[" << current_dep_num << "].flags.commutative = " << dependency_flags_concurrent << ";"
                            ;
                    }
                }

                Source dep_expr_addr;
                dep_expr_addr << as_expression(dep_expr.get_base_address());
                dependency_size << as_expression(dep_expr.get_sizeof());

                dependency_offset
                    << "((char*)(" << dep_expr_addr << ") - " << "(char*)" << arguments_accessor << it->get_field_name() << ")"
                    ;

                if (Nanos::Version::interface_is_at_least("master", 5001))
                {
                    dependency_flags 
                        << "{" 
                        << dependency_flags_in << "," 
                        << dependency_flags_out << ", "
                        << /* renaming has not yet been implemented */ "0, " 
                        << dependency_flags_concurrent
                        << "}"
                        ;
                }
                else
                {
                    dependency_flags 
                        << "{" 
                        << dependency_flags_in << "," 
                        << dependency_flags_out << ", "
                        << /* renaming has not yet been implemented */ "0, " 
                        << "}"
                        ;
                }

                int concurrent = ((dir & OutlineDataItem::DIRECTIONALITY_CONCURRENT) == OutlineDataItem::DIRECTIONALITY_CONCURRENT);

                dependency_flags_in << (((dir & OutlineDataItem::DIRECTIONALITY_IN) == OutlineDataItem::DIRECTIONALITY_IN) || concurrent);
                dependency_flags_out << (((dir & OutlineDataItem::DIRECTIONALITY_OUT) == OutlineDataItem::DIRECTIONALITY_OUT) || concurrent);
                dependency_flags_concurrent << concurrent;

                dependency_init.append_with_separator(current_dependency_init, ",");
            }
        }
    }
}

void LoweringVisitor::fill_dimensions(int n_dims, int actual_dim, int current_dep_num,
        Nodecl::NodeclBase * dim_sizes, 
        Type dep_type, 
        Source& dims_description, 
        Source& dependency_regions_code, 
        Scope sc)
{
    // We do not handle the contiguous dimension here
    if (actual_dim > 0)
    {
        fill_dimensions(n_dims, actual_dim - 1, current_dep_num, dim_sizes, dep_type.array_element(), dims_description, dependency_regions_code, sc);

        Source dimension_size, dimension_lower_bound, dimension_accessed_length;
        Nodecl::NodeclBase lb, ub, size;

        if (dep_type.array_is_region())
        {
            dep_type.array_get_region_bounds(lb, ub);
            size = dep_type.array_get_region_size();
        }
        else
        {
            dep_type.array_get_bounds(lb, ub);
            size = dep_type.array_get_size();
        }

        dimension_size << as_expression(dim_sizes[n_dims - actual_dim - 1].copy());
        dimension_lower_bound << as_expression(lb.copy());
        dimension_accessed_length << as_expression(size.copy());

        if (IS_C_LANGUAGE
                || IS_CXX_LANGUAGE)
        {
            dims_description << ", {" 
                << dimension_size << ", " 
                << dimension_lower_bound << ", "
                << dimension_accessed_length 
                << "}"
                ;
        }
        else if (IS_FORTRAN_LANGUAGE)
        {
            dependency_regions_code
                << "dimensions_" << current_dep_num << "[" << actual_dim << "].size = " << dimension_size << ";"
                << "dimensions_" << current_dep_num << "[" << actual_dim << "].lower_bound = " << dimension_lower_bound << ";"
                << "dimensions_" << current_dep_num << "[" << actual_dim << "].accessed_length = " << dimension_accessed_length << ";"
                ;
        }
    }
}

typedef std::map<TL::Symbol, Nodecl::NodeclBase> sym_to_argument_expr_t;

static Nodecl::NodeclBase rewrite_single_dependency(Nodecl::NodeclBase node, const sym_to_argument_expr_t& map)
{
    if (node.is_null())
        return node;

    if (node.is<Nodecl::Symbol>())
    {
        sym_to_argument_expr_t::const_iterator it = map.find(node.get_symbol());

        if (it != map.end())
        {
            return (it->second.copy());
        }
    }

    TL::ObjectList<Nodecl::NodeclBase> children = node.children();
    for (TL::ObjectList<Nodecl::NodeclBase>::iterator it = children.begin();
            it != children.end();
            it++)
    { 
        *it = rewrite_single_dependency(*it, map);
    }
    node.rechild(children);

    return node;
}

static TL::ObjectList<Nodecl::NodeclBase> rewrite_dependences(
        const TL::ObjectList<Nodecl::NodeclBase>& deps, 
        const sym_to_argument_expr_t& param_to_arg_expr)
{
    TL::ObjectList<Nodecl::NodeclBase> result;
    for (TL::ObjectList<Nodecl::NodeclBase>::const_iterator it = deps.begin();
            it != deps.end();
            it++)
    {
        Nodecl::NodeclBase copy = it->copy();
        result.append( rewrite_single_dependency(copy, param_to_arg_expr) );
    }

    return result;
}

static void copy_outline_data_item(
        OutlineDataItem& dest_info, 
        const OutlineDataItem& source_info,
        const sym_to_argument_expr_t& param_to_arg_expr)
{
    dest_info.set_sharing(source_info.get_sharing());
    dest_info.set_allocation_policy(source_info.get_allocation_policy());
    dest_info.set_directionality(source_info.get_directionality());

    dest_info.set_field_type(source_info.get_field_type());

    FORTRAN_LANGUAGE()
    {
        // We need an additional pointer due to pass by reference in Fortran
        dest_info.set_field_type(dest_info.get_field_type().get_lvalue_reference_to());
    }

    // Update dependences to reflect arguments as well
    dest_info.get_dependences() = rewrite_dependences(source_info.get_dependences(), param_to_arg_expr);
}

static void fill_map_parameters_to_arguments(
        TL::Symbol function,
        Nodecl::List arguments, 
        sym_to_argument_expr_t& param_to_arg_expr)
{
    int i = 0;
    for (Nodecl::List::iterator it = arguments.begin();
            it != arguments.end();
            it++, i++)
    {
        Nodecl::NodeclBase expression;
        TL::Symbol parameter_sym;
        if (it->is<Nodecl::FortranNamedPairSpec>())
        {
            // If this is a Fortran style argument use the symbol
            Nodecl::FortranNamedPairSpec named_pair(it->as<Nodecl::FortranNamedPairSpec>());

            param_to_arg_expr[named_pair.get_name().get_symbol()] = named_pair.get_argument();
        }
        else
        {
            // Get the i-th parameter of the function
            ERROR_CONDITION((function.get_related_symbols().size() <= i), "Too many parameters", 0);
            TL::Symbol parameter = function.get_related_symbols()[i];
            param_to_arg_expr[parameter] = *it;
        }
    }
}

void LoweringVisitor::visit(const Nodecl::Parallel::AsyncCall& construct)
{
    Nodecl::FunctionCall function_call = construct.get_call().as<Nodecl::FunctionCall>();
    ERROR_CONDITION(!function_call.get_called().is<Nodecl::Symbol>(), "Invalid ASYNC CALL!", 0);

    TL::Symbol called_sym = function_call.get_called().get_symbol();

    std::cerr << construct.get_locus() << ": note: call to task function '" << called_sym.get_qualified_name() << "'" << std::endl;

    // Get parameters outline info
    Nodecl::NodeclBase parameters_environment = construct.get_environment();
    OutlineInfo parameters_outline_info(parameters_environment, /* function_task */ true);

    // Fill arguments outline info using parameters
    OutlineInfo arguments_outline_info;
    Nodecl::List arguments = function_call.get_arguments().as<Nodecl::List>();

    TL::ObjectList<OutlineDataItem>& data_items  = parameters_outline_info.get_data_items();

    // This map associates every parameter symbol with its argument expression
    sym_to_argument_expr_t param_to_arg_expr;
    fill_map_parameters_to_arguments(called_sym, arguments, param_to_arg_expr);

    TL::ObjectList<TL::Symbol> new_arguments;
    Scope sc = construct.retrieve_context();
    for (sym_to_argument_expr_t::iterator it = param_to_arg_expr.begin();
            it != param_to_arg_expr.end();
            it++)
    {
        ObjectList<OutlineDataItem> found = data_items.find(functor(&OutlineDataItem::get_symbol), it->first);

        if (found.empty())
        {
            running_error("%s: error: cannot find parameter '%s' in OutlineInfo, this may be caused by bug #922\n", 
                    arguments.get_locus().c_str(),
                    it->first.get_name().c_str());
        }

        Counter& arg_counter = CounterManager::get_counter("nanos++-outline-arguments");
        std::stringstream ss;
        ss << "mcc_arg_" << (int)arg_counter;
        TL::Symbol new_symbol = sc.new_symbol(ss.str());
        arg_counter++;

        // FIXME - Wrap this sort of things
        new_symbol.get_internal_symbol()->kind = SK_VARIABLE;
        new_symbol.get_internal_symbol()->type_information = no_ref(it->first.get_internal_symbol()->type_information);

        new_arguments.append(new_symbol);

        OutlineDataItem& parameter_outline_data_item = parameters_outline_info.get_entity_for_symbol(it->first);

        OutlineDataItem& argument_outline_data_item = arguments_outline_info.get_entity_for_symbol(new_symbol);

        // Copy what must be copied from the parameter info
        copy_outline_data_item(argument_outline_data_item, parameter_outline_data_item, param_to_arg_expr);

        // This is a special kind of shared
        argument_outline_data_item.set_sharing(OutlineDataItem::SHARING_CAPTURE_ADDRESS);
        argument_outline_data_item.set_shared_expression(it->second);
    }

    // Craft a new function call with the new mcc_arg_X symbols
    TL::ObjectList<TL::Symbol>::iterator args_it = new_arguments.begin();
    TL::ObjectList<Nodecl::NodeclBase> arg_list;
    for (sym_to_argument_expr_t::iterator params_it = param_to_arg_expr.begin();
            params_it != param_to_arg_expr.end();
            params_it++, args_it++)
    {
        Nodecl::NodeclBase nodecl_arg = Nodecl::Symbol::make(*args_it, 
                function_call.get_filename(), 
                function_call.get_line());
        nodecl_arg.set_type( args_it->get_type() );

        // We must respect symbols in Fortran because of optional stuff
        if (IS_FORTRAN_LANGUAGE)
        {
            Nodecl::Symbol nodecl_param = Nodecl::Symbol::make(
                    params_it->first,
                    function_call.get_filename(), 
                    function_call.get_line());

            nodecl_arg = Nodecl::FortranNamedPairSpec::make(
                    nodecl_param,
                    nodecl_arg,
                    function_call.get_filename(), 
                    function_call.get_line());
        }

        arg_list.append(nodecl_arg);
    }

    Nodecl::List nodecl_arg_list = Nodecl::List::make(arg_list);

    Nodecl::NodeclBase expr_statement = 
                Nodecl::ExpressionStatement::make(
                    Nodecl::FunctionCall::make(
                        function_call.get_called().copy(),
                        nodecl_arg_list,
                        function_call.get_alternate_name().copy(),
                        Type::get_void_type(),
                        function_call.get_filename(),
                        function_call.get_line()),
                    function_call.get_filename(),
                    function_call.get_line());

    TL::ObjectList<Nodecl::NodeclBase> list_stmt; 
    list_stmt.append(expr_statement);

    Nodecl::NodeclBase statement = 
        Nodecl::Context::make(
                Nodecl::List::make(list_stmt),
                function_call.retrieve_context(),
                function_call.get_filename(),
                function_call.get_line()
                );

    Symbol function_symbol = Nodecl::Utils::get_enclosing_function(construct);

    emit_async_common(
            construct,
            function_symbol, 
            statement,
            /* priority */ Nodecl::NodeclBase::null(),
            /* is_untied */ false,

            arguments_outline_info);
}

} }
