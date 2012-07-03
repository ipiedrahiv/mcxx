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




#include "tl-source.hpp"
#include "tl-scope.hpp"
#include "tl-nodecl.hpp"

#include "cxx-exprtype.h"
#include "cxx-ambiguity.h"
#include "cxx-printscope.h"
#include "cxx-utils.h"
#include "cxx-parser.h"
#include "c99-parser.h"
#include "fortran03-lexer.h"
#include "fortran03-parser.h"
#include "fortran03-buildscope.h"
#include "fortran03-exprtype.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace TL
{
    SourceLanguage Source::source_language;

    ReferenceScope::ReferenceScope(Scope sc)
        : _scope(sc)
    {
    }

    ReferenceScope::ReferenceScope(Nodecl::NodeclBase n)
        : _scope(n.retrieve_context())
    {
    }

    Scope ReferenceScope::get_scope() const
    {
        return _scope;
    }

    std::string SourceRef::get_source() const
    {
        return _src->get_source(false);
    }

    void Source::append_text_chunk(const std::string& str)
    {
        if (_chunk_list->empty())
        {
            _chunk_list->push_back(SourceChunkRef(new SourceText(str)));
        }
        else
        {
            SourceChunkRef last = *(_chunk_list->rbegin());

            if (last->is_source_text())
            {
                RefPtr<SourceText> text = RefPtr<SourceText>::cast_dynamic(last);
                text->_source += str;
            }
            else
            {
                _chunk_list->push_back(SourceChunkRef(new SourceText(str)));
            }
        }
    }

    void Source::append_source_ref(SourceChunkRef ref)
    {
        _chunk_list->push_back(ref);
    }

    Source& Source::operator<<(const std::string& str)
    {
        append_text_chunk(str);
        return *this;
    }

    Source& Source::operator<<(int num)
    {
        std::stringstream ss;
        ss << num;
        append_text_chunk(ss.str());
        return *this;
    }

    Source& Source::operator<<(Source& src)
    {
        RefPtr<Source> ref_src = RefPtr<Source>(new Source(src));

        SourceChunkRef new_src = SourceChunkRef(new SourceRef(ref_src));

        append_source_ref(new_src);
        return *this;
    }

    Source& Source::operator<<(RefPtr<Source> src)
    {
        SourceChunkRef new_src = SourceChunkRef(new SourceRef(src));

        append_source_ref(new_src);
        return *this;
    }

    Source::operator std::string()
    {
        return this->get_source(false);
    }

    std::string Source::get_source(bool with_newlines) const
    {
        std::string temp_result;
        for(ObjectList<SourceChunkRef>::const_iterator it = _chunk_list->begin();
                it != _chunk_list->end();
                it++)
        {
            temp_result += (*it)->get_source();
        }

        if (!with_newlines)
        {
            return temp_result;
        }

        std::string result;
        // Eases debugging
        bool preprocessor_line = false;
        bool inside_string = false;
        char current_string_delimiter = ' ';

        for (unsigned int i = 0; i < temp_result.size(); i++)
        {
            char c = temp_result[i];

            bool add_new_line = false;

            switch (c)
            {
                case '\t':
                case ' ':
                    {
                        break;
                    }
                case '\'':
                case '"':
                    {
                        if (!inside_string)
                        {
                            inside_string = true;
                            current_string_delimiter = c;
                        }
                        else
                        {
                            if (c == current_string_delimiter
                                    && ((i == 1 && temp_result[i-1] != '\\')
                                        || (i > 1 && 
                                            (temp_result[i-1] != '\\'
                                             || temp_result[i-2] == '\\')))
                               )
                            {
                                inside_string = false;
                            }
                        }
                        break;
                    }
                case '#':
                    {
                        preprocessor_line = true;
                        break;
                    }
                case ';':
                case '{':
                case '}':
                    {
                        if (!inside_string
                                && !preprocessor_line
                                && !IS_FORTRAN_LANGUAGE)
                        {
                            add_new_line = true;
                        }
                        break;
                    }
                case '\n':
                    {
                        // Maybe it's being continuated
                        if (i == 0
                                || temp_result[i-1] != '\\')
                        {
                            preprocessor_line = false;
                        }

                        break;
                    }
                default:
                    {
                        break;
                    }
            }

            result += c;

            if (add_new_line)
            {
                result += '\n';
            }
        }

        return result;
    }

    bool Source::operator==(const Source& src) const
    {
        return this->get_source() == src.get_source();
    }

    bool Source::operator!=(const Source &src) const
    {
        return !(this->operator==(src));
    }

    bool Source::operator<(const Source &src) const
    {
        return this->get_source() < src.get_source();
    }

    Source& Source::operator=(const Source& src)
    {
        if (this != &src)
        {
            // The same as *(_chunk_list.operator->()) = *(src._chunk_list.operator->()); but clearer
            _chunk_list->clear();
            for(ObjectList<SourceChunkRef>::const_iterator it = src._chunk_list->begin();
                    it != src._chunk_list->end();
                    it++)
            {
                _chunk_list->push_back(*it);
            }
        }
        return (*this);
    }

    static bool string_is_blank(const std::string& src)
    {
        for (std::string::const_iterator it = src.begin();
                it != src.end();
                it++)
        {
            if (*it != ' '
                    || *it != '\t')
            {
                return false;
            }
        }
        return true;
    }

    Source& Source::append_with_separator(const std::string& src, const std::string& separator)
    {
        if (!string_is_blank(src))
        {
            if (all_blanks())
            {
                append_text_chunk(src);
            }
            else
            {
                append_text_chunk(separator + src);
            }
        }

        return (*this);
    }

    Source& Source::append_with_separator(Source& src, const std::string& separator)
    {
        if (!src.all_blanks())
        {
            if (!all_blanks())
            {
                append_text_chunk(separator);
            }
            RefPtr<Source> ref_source = RefPtr<Source>(new Source(src));
            append_source_ref(SourceChunkRef(new SourceRef(ref_source)));
        }

        return (*this);
    }

    bool Source::empty() const
    {
        return all_blanks();
    }

    bool Source::all_blanks() const
    {
        if (_chunk_list->empty())
            return true;

        std::string str = this->get_source();
        return string_is_blank(str);
    }

    std::string comment(const std::string& str)
    {
        std::string result;

        result = "@-C-@" + str + "@-CC-@";
        return result;
    }

    std::string line_marker(const std::string& filename, int line)
    {
        std::stringstream ss;

       ss << "#line " << line;

       if (filename == "")
       {
           ss << "\n";
       }
       else
       {
           ss << "\"" << filename << "\"\n";
       }
       
       return ss.str();
    }
    
    // This is quite inefficient but will do
    std::string Source::format_source(const std::string& src)
    {
        int line = 1;

        std::stringstream ss;

        ss << "[" << std::setw(5) << line << std::setw(0) << "] ";


        for (std::string::const_iterator it = src.begin();
                it != src.end();
                it++)
        {
            ss << *it;
            if (*it == '\n')
            {
                line++;
                ss << "[" << std::setw(5) << line << std::setw(0) << "] ";
            }
        }

        return ss.str();
    }

    static decl_context_t decl_context_identity(decl_context_t d)
    {
        return d;
    }

    static decl_context_t decl_context_namespace(decl_context_t d)
    {
        d.current_scope = d.namespace_scope;
        d.function_scope = NULL;
        d.block_scope = NULL;

        return d;
    }

    static decl_context_t decl_context_global(decl_context_t d)
    {
        d.current_scope = d.global_scope;
        d.namespace_scope = d.global_scope;
        d.function_scope = NULL;
        d.block_scope = NULL;

        return d;
    }

    static decl_context_t decl_context_program_unit(decl_context_t d)
    {
        return d.current_scope->related_entry->decl_context;
    }

    void Source::switch_language(source_language_t& lang)
    {
        lang = CURRENT_CONFIGURATION->source_language;
        switch ((int)Source::source_language.get_language())
        {
            case SourceLanguage::C :
            {
                CURRENT_CONFIGURATION->source_language = SOURCE_LANGUAGE_C;
                break;
            }
            case SourceLanguage::CPlusPlus :
            {
                CURRENT_CONFIGURATION->source_language = SOURCE_LANGUAGE_CXX;
                break;
            }
            case SourceLanguage::Fortran :
            {
                CURRENT_CONFIGURATION->source_language = SOURCE_LANGUAGE_FORTRAN;
                break;
            }
            default:
            {
                // Do nothing
            }
        }
    }

    void Source::restore_language(source_language_t lang)
    {
        CURRENT_CONFIGURATION->source_language = lang;
    }

    Nodecl::NodeclBase Source::parse_generic(ReferenceScope ref_scope,
            ParseFlags parse_flags,
            const std::string& subparsing_prefix,
            prepare_lexer_fun_t prepare_lexer,
            parse_fun_t parse,
            compute_nodecl_fun_t compute_nodecl,
            decl_context_map_fun_t decl_context_map_fun)
    {

        source_language_t kept_language;
        switch_language(kept_language);

        std::string mangled_text = subparsing_prefix + " " + this->get_source(true);

        prepare_lexer(mangled_text.c_str());

        int parse_result = 0;
        AST a = NULL;

        parse_result = parse(&a);

        if (parse_result != 0)
        {
            running_error("Could not parse source\n\n%s\n", 
                    format_source(this->get_source(true)).c_str());
        }

        decl_context_t decl_context = decl_context_map_fun(ref_scope.get_scope().get_decl_context());

        nodecl_t nodecl_output = nodecl_null();
        compute_nodecl(a, decl_context, &nodecl_output);

        restore_language(kept_language);

        return nodecl_output;
    }

    Nodecl::NodeclBase Source::parse_global(ReferenceScope ref_scope, ParseFlags parse_flags)
    {
        switch ((int)this->source_language.get_language())
        {
            case SourceLanguage::C :
            {
                return parse_generic(ref_scope, parse_flags, "@DECLARATION@", 
                        mc99_prepare_string_for_scanning,
                        mc99parse,
                        build_scope_declaration_sequence,
                        decl_context_global);
                break;
            }
            case SourceLanguage::CPlusPlus :
            {
                return parse_generic(ref_scope, parse_flags, "@DECLARATION@", 
                        mcxx_prepare_string_for_scanning,
                        mcxxparse,
                        build_scope_declaration_sequence,
                        decl_context_global);
                break;
            }
            case SourceLanguage::Fortran :
            {
                return parse_generic(ref_scope, parse_flags, "@PROGRAM-UNIT@", 
                        mf03_prepare_string_for_scanning,
                        mf03parse,
                        build_scope_program_unit_seq,
                        decl_context_global
                        );
                break;
            }
            default:
            {
                internal_error("Code unreachable", 0);
            }
        }

        return Nodecl::NodeclBase::null();
    }

    Nodecl::NodeclBase Source::parse_declaration(ReferenceScope ref_scope, ParseFlags parse_flags)
    {
        switch ((int)this->source_language.get_language())
        {
            case SourceLanguage::C :
            {
                return parse_generic(ref_scope, parse_flags, "@DECLARATION@", 
                        mc99_prepare_string_for_scanning,
                        mc99parse,
                        build_scope_declaration_sequence,
                        decl_context_namespace);
                break;
            }
            case SourceLanguage::CPlusPlus :
            {
                return parse_generic(ref_scope, parse_flags, "@DECLARATION@", 
                        mcxx_prepare_string_for_scanning,
                        mcxxparse,
                        build_scope_declaration_sequence,
                        decl_context_namespace);
                break;
            }
            case SourceLanguage::Fortran :
            {
                return parse_generic(ref_scope, parse_flags, "@PROGRAM-UNIT@", 
                        mf03_prepare_string_for_scanning,
                        mf03parse,
                        build_scope_program_unit_seq,
                        decl_context_program_unit
                        );
                break;
            }
            default:
            {
                internal_error("Code unreachable", 0);
            }
        }

        return Nodecl::NodeclBase::null();
    }

    Nodecl::NodeclBase Source::parse_statement(ReferenceScope ref_scope, ParseFlags parse_flags)
    {
        switch ((int)this->source_language.get_language())
        {
            case SourceLanguage::C :
            {
                return parse_generic(ref_scope, parse_flags, "@STATEMENT@", 
                        mc99_prepare_string_for_scanning,
                        mc99parse,
                        build_scope_statement,
                        decl_context_identity);
                break;
            }
            case SourceLanguage::CPlusPlus :
            {
                return parse_generic(ref_scope, parse_flags, "@STATEMENT@", 
                        mcxx_prepare_string_for_scanning,
                        mcxxparse,
                        build_scope_statement,
                        decl_context_identity);
                break;
            }
            case SourceLanguage::Fortran :
            {
                return parse_generic(ref_scope, parse_flags, "@STATEMENT@", 
                        mf03_prepare_string_for_scanning,
                        mf03parse,
                        fortran_build_scope_statement,
                        decl_context_identity);
                break;
            }
            default:
            {
                internal_error("Code unreachable", 0);
            }
        }

        return Nodecl::NodeclBase::null();
    }

    static void c_cxx_check_expression_adaptor_(AST a, decl_context_t decl_context, nodecl_t* nodecl_output)
    {
        ::check_expression(a, decl_context, nodecl_output);
    }

    static void fortran_check_expression_adaptor_(AST a, decl_context_t decl_context, nodecl_t* nodecl_output)
    {
        if (ASTType(a) == AST_COMMON_NAME)
        {
            // We allow common names in expressions
            scope_entry_t* entry = ::query_common_name(decl_context, ASTText(ASTSon0(a)),
                    ASTFileName(ASTSon0(a)), ASTLine(ASTSon0(a)));
            if (entry != NULL)
            {
                *nodecl_output = ::nodecl_make_symbol(entry, ASTFileName(a), ASTLine(a));
            }
            else
            {
                *nodecl_output = ::nodecl_make_err_expr(ASTFileName(a), ASTLine(a));
            }
        }
        else
        {
            ::fortran_check_expression(a, decl_context, nodecl_output);
        }
    }

    Nodecl::NodeclBase Source::parse_expression(ReferenceScope ref_scope, ParseFlags parse_flags)
    {
        switch ((int)this->source_language.get_language())
        {
            case SourceLanguage::C :
            {
                return parse_generic(ref_scope, parse_flags, "@EXPRESSION@", 
                        mc99_prepare_string_for_scanning,
                        mc99parse,
                        c_cxx_check_expression_adaptor_,
                        decl_context_identity);
                break;
            }
            case SourceLanguage::CPlusPlus :
            {
                return parse_generic(ref_scope, parse_flags, "@EXPRESSION@", 
                        mcxx_prepare_string_for_scanning,
                        mcxxparse,
                        c_cxx_check_expression_adaptor_,
                        decl_context_identity);
                break;
            }
            case SourceLanguage::Fortran :
            {
                return parse_generic(ref_scope, parse_flags, "@EXPRESSION@", 
                        mf03_prepare_string_for_scanning,
                        mf03parse,
                        fortran_check_expression_adaptor_,
                        decl_context_identity);
                break;
            }
            default:
            {
                internal_error("Code unreachable", 0);
            }
        }

        return Nodecl::NodeclBase::null();
    }

    std::string preprocessor_line(const std::string& str)
    {
        std::string result;

        result = "@-P-@" + str + "@-PP-@";
        return result;
    }

    std::string to_string(const ObjectList<std::string>& t, const std::string& separator)
    {
        std::string result;

        for (ObjectList<std::string>::const_iterator it = t.begin();
                it != t.end();
                it++)
        {
            if (it != t.begin())
            {
                result = result + separator;
            }

            result = result + (*it);
        }

        return result;
    }

    std::string as_expression(const Nodecl::NodeclBase& n)
    {
        return nodecl_expr_to_source(n.get_internal_nodecl());
    }

    std::string as_statement(const Nodecl::NodeclBase& n)
    {
        return nodecl_stmt_to_source(n.get_internal_nodecl());
    }

    std::string as_type(TL::Type t)
    {
        return type_to_source(t.get_internal_type());
    }

    std::string as_symbol(TL::Symbol s)
    {
        return symbol_to_source(s.get_internal_symbol());
    }

    std::string statement_placeholder(Nodecl::NodeclBase& placeholder)
    {
        std::stringstream ss;
        ss << "@STATEMENT-PH::" << placeholder.get_internal_tree_address() << "@";
        return ss.str();
    }
}
