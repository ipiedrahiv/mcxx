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



#include "cxx-cexpr.h"
#include "cxx-codegen.h"

#include "tl-cfg-renaming-visitor.hpp"
#include "tl-nodecl-utils.hpp"
#include "tl-nodecl-calc.hpp"


namespace TL
{
    namespace Analysis
    {
        CfgRenamingVisitor::CfgRenamingVisitor(std::map<Symbol, Nodecl::NodeclBase> rename_map, const char* filename, int line)
            : _rename_map(rename_map), _filename(filename), _line(line), _s(NULL), _computing_limits(false)
        {}
        
        CfgRenamingVisitor::CfgRenamingVisitor(const CfgRenamingVisitor& rename_v)
        {
            _rename_map = rename_v._rename_map;
            _filename = rename_v._filename;
            _line = rename_v._line;
            _s = rename_v._s;
            _computing_limits = rename_v._computing_limits;
        }
        
        Symbol CfgRenamingVisitor::get_matching_symbol() const
        {
            return _s;
        }
        
        void CfgRenamingVisitor::set_computing_range_limits(bool computing_limits)
        {
            _computing_limits = computing_limits;
        }
        
        nodecl_t CfgRenamingVisitor::create_nodecl_list(ObjectList<Nodecl::NodeclBase> list)
        {
            nodecl_t nodecl_l = nodecl_null();
            for (ObjectList<Nodecl::NodeclBase>::iterator it = list.begin();
                it != list.end();
                ++it)
            {
                nodecl_l = nodecl_append_to_list(nodecl_l, it->get_internal_nodecl());
            }
            
            return nodecl_l;
        }        
        
        // FIXME We should evaluate the type of both nodes, not just node 'a'
        static Nodecl::NodeclBase min(Nodecl::NodeclBase a, Nodecl::NodeclBase b)
        {
            Nodecl::Calculator calc;
            const_value_t* a_const_val = calc.compute_const_value(a);
            const_value_t* b_const_val = calc.compute_const_value(b);
            
            Nodecl::NodeclBase result;
            if (a_const_val != NULL && b_const_val != NULL)
            {
                const_value_t* const_val = (a_const_val < b_const_val) ? a_const_val : b_const_val;
                if (a.is<Nodecl::IntegerLiteral>())
                {
                    result = Nodecl::IntegerLiteral::make(a.get_type(), const_val, a.get_filename(), a.get_line());
                }
                else if (a.is<Nodecl::FloatingLiteral>())
                {
                    result = Nodecl::FloatingLiteral::make(a.get_type(), const_val, a.get_filename(), a.get_line());
                }
                else
                {
                    internal_error("Unexpected node type '%s' while computing initial value in a constant expression", 
                                ast_print_node_type(a.get_kind()));
                }             
                return result;
            }
            else
            {
                return Nodecl::NodeclBase::null();
            }        
        }
        
        static Nodecl::NodeclBase max(Nodecl::NodeclBase a, Nodecl::NodeclBase b)
        {
            Nodecl::Calculator calc;
            const_value_t* a_const_val = calc.compute_const_value(a);
            const_value_t* b_const_val = calc.compute_const_value(b);
            
            Nodecl::NodeclBase result;
            if (a_const_val != NULL && b_const_val != NULL)
            {
                const_value_t* const_val = (a_const_val > b_const_val) ? a_const_val : b_const_val;
                if (a.is<Nodecl::IntegerLiteral>())
                {
                    result = Nodecl::IntegerLiteral::make(a.get_type(), const_val, a.get_filename(), a.get_line());
                }
                else if (a.is<Nodecl::FloatingLiteral>())
                {
                    result = Nodecl::FloatingLiteral::make(a.get_type(), const_val, a.get_filename(), a.get_line());
                }
                else
                {
                    internal_error("Unexpected node type '%s' while computing initial value in a constant expression", 
                                ast_print_node_type(a.get_kind()));
                }             
                return result;
            }
            else
            {
                return Nodecl::NodeclBase::null();
            }         
        }
        
        Nodecl::NodeclBase CfgRenamingVisitor::combine_variable_values(Nodecl::NodeclBase node1, Nodecl::NodeclBase node2)
        {
            if (node1.is<Nodecl::Range>() && node2.is<Nodecl::Range>())
            {
                Nodecl::Range node1_range = node1.as<Nodecl::Range>();
                Nodecl::NodeclBase lb1 = node1_range.get_lower();
                Nodecl::NodeclBase ub1 = node1_range.get_upper();
                Nodecl::NodeclBase stride1 = node1_range.get_stride();
                
                Nodecl::Range node2_range = node2.as<Nodecl::Range>();
                Nodecl::NodeclBase lb2 = node2_range.get_lower();
                Nodecl::NodeclBase ub2 = node2_range.get_upper();
                Nodecl::NodeclBase stride2 = node2_range.get_stride();
                
                // Mix the strides
                Nodecl::NodeclBase stride;
                if (!stride1.is_null() && !stride2.is_null())
                {
                    if (Nodecl::Utils::equal_nodecls(stride1, stride2))
                    {
                        stride = stride1;
                    }
                    else
                    {   // FIXME When both are a range and the renamed var is different, we have nested loops!
                        internal_error("Combination of induction variables coming from nested loops not yet implemented" \
                                        " when ranging induction variables", 0);
                    }
                }
                else if (!stride1.is_null())
                {
                    stride = stride1;
                }
                else
                {
                    stride = stride2;
                }        
            
                // We must keep the smaller LB and the bigger UB
                Nodecl::NodeclBase lb = min(lb1, lb2);
                Nodecl::NodeclBase ub = max(ub1, ub2);
                
                if (lb.is_null() || ub.is_null())
                {
                    return Nodecl::NodeclBase::null();
                }
            
                return Nodecl::Range::make(lb, ub, stride, node1.get_type(), node1.get_filename(), node1.get_line());
            }
            else if (!node1.is<Nodecl::Range>() && !node2.is<Nodecl::Range>())
            {
                if (Nodecl::Utils::equal_nodecls(node1, node2))
                {
                    return node1;
                }
                else
                {
                    return Nodecl::NodeclBase::null();
                }
            }
            else
            {
                Nodecl::NodeclBase lb, ub, l, stride;
                
                if (node1.is<Nodecl::Range>())
                {
                    Nodecl::Range node1_range = node1.as<Nodecl::Range>();
                    lb = node1_range.get_lower();
                    ub = node1_range.get_upper();
                    stride = node1_range.get_stride();
                    l = node2;
                }
                else
                {
                    Nodecl::Range node2_range = node2.as<Nodecl::Range>();
                    lb = node2_range.get_lower();
                    ub = node2_range.get_upper();
                    stride = node2_range.get_stride();
                    l = node1;
                }
            
                if (l.is_constant() && stride.is_constant())
                {
                    int l_val = const_value_cast_to_4(l.get_constant());
                    int stride_val = const_value_cast_to_4(stride.get_constant());
                    
                    if (lb.is_constant())
                    {
                        int lb_val = const_value_cast_to_4(lb.get_constant());
                        if (l_val < lb_val && stride_val > 0)
                        {
                            return Nodecl::Range::make(l, ub, stride, node1.get_type(), node1.get_filename(), node1.get_line());
                        }
                        else
                        {
                            return Nodecl::NodeclBase::null();
                        }
                    }
                    else if (ub.is_constant())
                    {
                        int ub_val = const_value_cast_to_4(ub.get_constant());
                        if (l_val > ub_val && stride_val < 0)
                        {
                            return Nodecl::Range::make(lb, l, stride, node1.get_type(), node1.get_filename(), node1.get_line());
                        }
                        else
                        {
                            return Nodecl::NodeclBase::null();
                        }
                    }
                    else
                    {
                        return Nodecl::NodeclBase::null();
                    }
                }
                else
                {
                    return Nodecl::NodeclBase::null();
                }
            }
        }    
        
        //! Useful to combine expressions as 'i = i + j;' where 'i, j' are induction variables
        template <typename T>
        Nodecl::NodeclBase CfgRenamingVisitor::create_new_range_from_binary_node(T& n, Nodecl::NodeclBase lb1, Nodecl::NodeclBase ub1,
                                                                                Nodecl::NodeclBase lb2, Nodecl::NodeclBase ub2,
                                                                                Nodecl::NodeclBase stride)
        {
            Nodecl::NodeclBase renamed, lb, ub;
            
            if (n.template is<Nodecl::Add>())
            {
                lb = Nodecl::Add::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::Add::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::Minus>())
            {
                lb = Nodecl::Minus::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::Minus::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::Mul>())
            {
                lb = Nodecl::Mul::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::Mul::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::Div>())
            {
                lb = Nodecl::Div::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::Div::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::Mod>())
            {
                lb = Nodecl::Mod::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::Mod::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::Power>())
            {
                lb = Nodecl::Power::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::Power::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::LogicalAnd>())
            {
                lb = Nodecl::LogicalAnd::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::LogicalAnd::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::LogicalOr>())
            {
                lb = Nodecl::LogicalOr::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::LogicalOr::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::BitwiseAnd>())
            {
                lb = Nodecl::BitwiseAnd::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::BitwiseAnd::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::BitwiseOr>())
            {
                lb = Nodecl::BitwiseOr::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::BitwiseOr::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::BitwiseXor>())
            {
                lb = Nodecl::BitwiseXor::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::BitwiseXor::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::Shr>())
            {
                lb = Nodecl::Shr::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::Shr::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::Shl>())
            {
                lb = Nodecl::Shl::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::Shl::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::Assignment>())
            {
                lb = Nodecl::Assignment::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::Assignment::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::AddAssignment>())
            {
                lb = Nodecl::AddAssignment::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::AddAssignment::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::MinusAssignment>())
            {
                lb = Nodecl::MinusAssignment::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::MinusAssignment::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::DivAssignment>())
            {
                lb = Nodecl::DivAssignment::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::DivAssignment::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::MulAssignment>())
            {
                lb = Nodecl::MulAssignment::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::MulAssignment::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::ModAssignment>())
            {
                lb = Nodecl::ModAssignment::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::ModAssignment::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::BitwiseAndAssignment>())
            {
                lb = Nodecl::BitwiseAndAssignment::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::BitwiseAndAssignment::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::BitwiseOrAssignment>())
            {
                lb = Nodecl::BitwiseOrAssignment::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::BitwiseOrAssignment::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::BitwiseXorAssignment>())
            {
                lb = Nodecl::BitwiseXorAssignment::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::BitwiseXorAssignment::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::ShrAssignment>())
            {
                lb = Nodecl::ShrAssignment::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::ShrAssignment::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::ShlAssignment>())
            {
                lb = Nodecl::ShlAssignment::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::ShlAssignment::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::Equal>())
            {
                lb = Nodecl::Equal::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::Equal::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::Different>())
            {
                lb = Nodecl::Different::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::Different::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::LowerThan>())
            {
                lb = Nodecl::LowerThan::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::LowerThan::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::GreaterThan>())
            {
                lb = Nodecl::GreaterThan::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::GreaterThan::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::LowerOrEqualThan>())
            {
                lb = Nodecl::LowerOrEqualThan::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::LowerOrEqualThan::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::GreaterOrEqualThan>())
            {
                lb = Nodecl::GreaterOrEqualThan::make(lb1, lb2, n.get_type(), _filename, _line);
                ub = Nodecl::GreaterOrEqualThan::make(ub1, ub2, n.get_type(), _filename, _line);
            }
            else
            {            
                internal_error("Unexpected node type '%s' while renaming ranged binary node", ast_print_node_type(n.get_kind()));
            }
            
            // Compute constant values for LB and UB if possible
            Nodecl::NodeclBase reduced_lb = Nodecl::Utils::reduce_expression(lb);
//             std::cerr << "Reducing LB: " << lb.prettyprint() << "  --> " << reduced_lb.prettyprint() << std::endl;
            Nodecl::NodeclBase reduced_ub = Nodecl::Utils::reduce_expression(ub);
//             std::cerr << "Reducing UB: " << ub.prettyprint() << "  --> " << reduced_ub.prettyprint() << std::endl;
            
            renamed = Nodecl::Range::make(reduced_lb, reduced_ub, stride, n.get_type(), _filename, _line);
            
            return renamed;
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::unhandled_node(const Nodecl::NodeclBase& n)
        {
            Nodecl::NodeclBase unhandled_n = n;
            internal_error("Unhandled node while Renaming '%s' of type '%s'", unhandled_n.prettyprint().c_str(), ast_print_node_type(n.get_kind()));
        }    
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::Symbol& n)
        {
            if (_rename_map.find(n.get_symbol()) != _rename_map.end())
            {
                Nodecl::NodeclBase mapped_value = _rename_map[n.get_symbol()];
                _s = n.get_symbol();
                return ObjectList<Nodecl::NodeclBase>(1, mapped_value);
            }
            
            return ObjectList<Nodecl::NodeclBase>();
        }

        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::ArraySubscript& n)
        {
            Nodecl::NodeclBase array = n;
            Nodecl::NodeclBase subscripted = n.get_subscripted();
            Nodecl::NodeclBase subscripts = n.get_subscripts();
            
            ObjectList<Nodecl::NodeclBase> renamed_subscripted = walk(subscripted);
            ObjectList<Nodecl::NodeclBase> renamed_subscripts, aux_subscripts;
            if (subscripts.is<Nodecl::List>())
            {
                Nodecl::List subscripts_l = subscripts.as<Nodecl::List>();
                for(Nodecl::List::iterator it = subscripts_l.begin(); it != subscripts_l.end(); ++it)
                {
                    ObjectList<Nodecl::NodeclBase> aux = walk(*it);
                    if (aux.empty())
                    {
                        renamed_subscripts.append(*it);
                        aux_subscripts.append(*it);
                    }
                    else
                    {
                        renamed_subscripts.append(aux[0]);
                    }
                }
                if (renamed_subscripts == aux_subscripts)
                {   // No renaming has been performed
                    renamed_subscripts.clear();
                }
            }
            else
            {
                renamed_subscripts = walk(subscripts);
            }
            
            if (!renamed_subscripted.empty() || !renamed_subscripts.empty())
            {
                if (!renamed_subscripted.empty())
                {
                    subscripted = renamed_subscripted[0];
                }
                if (!renamed_subscripts.empty())
                {
                    subscripts = Nodecl::NodeclBase(create_nodecl_list(renamed_subscripts));
                }
                
                Nodecl::NodeclBase renamed = Nodecl::ArraySubscript::make(subscripted, subscripts, n.get_type(), _filename, _line);
                return ObjectList<Nodecl::NodeclBase>(1, renamed);
            }
            
            return ObjectList<Nodecl::NodeclBase>();
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::Range& n)
        {
            Nodecl::NodeclBase lower = n.get_lower();
            Nodecl::NodeclBase upper = n.get_upper();
            Nodecl::NodeclBase stride = n.get_stride();
            
            if (_computing_limits)
            {
                if (stride.is_constant())
                {
                    Nodecl::NodeclBase renamed;
                    const_value_t* stride_c_val = nodecl_get_constant(stride.get_internal_nodecl());
                    if (const_value_is_positive(stride_c_val))
                    {   // We keep the UB
                        renamed = upper;
                    }
                    else
                    {   // We keep the LB
                        renamed = lower;
                    }
                    return ObjectList<Nodecl::NodeclBase>(1, renamed);
                }
                else
                {
                    internal_error("Non constant strides not yet implemented to compute the value of a range at the loop ending", 0);
                }
            }
            else
            {
                ObjectList<Nodecl::NodeclBase> renamed_lower = walk(lower);
                ObjectList<Nodecl::NodeclBase> renamed_upper = walk(upper);
                ObjectList<Nodecl::NodeclBase> renamed_stride = walk(stride);

                if (!renamed_lower.empty() || !renamed_upper.empty() || !renamed_stride.empty())
                {
                    if (!renamed_lower.empty())
                    {
                        lower = renamed_lower[0];
                    }
                    if (!renamed_upper.empty())
                    {
                        upper = renamed_upper[0];
                    }
                    if (!renamed_stride.empty())
                    {
                        stride = renamed_stride[0];
                    }
                    
                    Nodecl::NodeclBase renamed = Nodecl::Range::make(lower, upper, stride, n.get_type(), _filename, _line);
                    return ObjectList<Nodecl::NodeclBase>(1, renamed);
                }
            } 
            
            return ObjectList<Nodecl::NodeclBase>();
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::ClassMemberAccess& n)
        {
            Nodecl::NodeclBase lhs = n.get_lhs();
            Nodecl::NodeclBase member = n.get_member();
            
            ObjectList<Nodecl::NodeclBase> renamed_lhs = walk(lhs);
            ObjectList<Nodecl::NodeclBase> renamed_member = walk(member);
            
            if (!renamed_lhs.empty() || !renamed_member.empty())
            {
                if (!renamed_lhs.empty())
                {
                    lhs = renamed_lhs[0];
                }
                if (!renamed_member.empty())
                {
                    member = renamed_member[0];
                }

                Nodecl::NodeclBase renamed = Nodecl::ClassMemberAccess::make(lhs, member, n.get_type(), _filename, _line);
                return ObjectList<Nodecl::NodeclBase>(1, renamed);
            }
            
            return ObjectList<Nodecl::NodeclBase>();
        }

        template <typename T>
        Nodecl::NodeclBase CfgRenamingVisitor::create_new_binary_node(T& n, Nodecl::NodeclBase lhs, Nodecl::NodeclBase rhs)
        {
            Nodecl::NodeclBase renamed;
            
            if (n.template is<Nodecl::Add>())
            {
                renamed = Nodecl::Add::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::Minus>())
            {
                renamed = Nodecl::Minus::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::Mul>())
            {
                renamed = Nodecl::Mul::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::Div>())
            {
                renamed = Nodecl::Div::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::Mod>())
            {
                renamed = Nodecl::Mod::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::Power>())
            {
                renamed = Nodecl::Power::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::LogicalAnd>())
            {
                renamed = Nodecl::LogicalAnd::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::LogicalOr>())
            {
                renamed = Nodecl::LogicalOr::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::BitwiseAnd>())
            {
                renamed = Nodecl::BitwiseAnd::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::BitwiseOr>())
            {
                renamed = Nodecl::BitwiseOr::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::BitwiseXor>())
            {
                renamed = Nodecl::BitwiseXor::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::Shr>())
            {
                renamed = Nodecl::Shr::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::Shl>())
            {
                renamed = Nodecl::Shl::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::Assignment>())
            {
                renamed = Nodecl::Assignment::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::AddAssignment>())
            {
                renamed = Nodecl::AddAssignment::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::MinusAssignment>())
            {
                renamed = Nodecl::MinusAssignment::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::DivAssignment>())
            {
                renamed = Nodecl::DivAssignment::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::MulAssignment>())
            {
                renamed = Nodecl::MulAssignment::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::ModAssignment>())
            {
                renamed = Nodecl::ModAssignment::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::BitwiseAndAssignment>())
            {
                renamed = Nodecl::BitwiseAndAssignment::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::BitwiseOrAssignment>())
            {
                renamed = Nodecl::BitwiseOrAssignment::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::BitwiseXorAssignment>())
            {
                renamed = Nodecl::BitwiseXorAssignment::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::ShrAssignment>())
            {
                renamed = Nodecl::ShrAssignment::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::ShlAssignment>())
            {
                renamed = Nodecl::ShlAssignment::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::Equal>())
            {
                renamed = Nodecl::Equal::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::Different>())
            {
                renamed = Nodecl::Different::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::LowerThan>())
            {
                renamed = Nodecl::LowerThan::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::GreaterThan>())
            {
                renamed = Nodecl::GreaterThan::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::LowerOrEqualThan>())
            {
                renamed = Nodecl::LowerOrEqualThan::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else if (n.template is<Nodecl::GreaterOrEqualThan>())
            {
                renamed = Nodecl::GreaterOrEqualThan::make(lhs, rhs, n.get_type(), _filename, _line);
            }
            else
            {
                internal_error("Unexpected node type '%s' while renaming binary node", ast_print_node_type(n.get_kind()));
            }
            
            return renamed;
        }

        template <typename T>
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit_binary(const T& n)
        {
            Nodecl::NodeclBase lhs = n.get_lhs();
            Nodecl::NodeclBase rhs = n.get_rhs();
            
            ObjectList<Nodecl::NodeclBase> renamed_lhs = walk(lhs);
            ObjectList<Nodecl::NodeclBase> renamed_rhs = walk(rhs);
            
            Nodecl::NodeclBase renamed;
            if (!renamed_lhs.empty() || !renamed_rhs.empty())
            {
                if (!renamed_lhs.empty())
                {
                    lhs = renamed_lhs[0];
                }
                if (!renamed_rhs.empty())
                {
                    rhs = renamed_rhs[0];
                }
                
                if (lhs.is<Nodecl::Range>() || rhs.is<Nodecl::Range>())
                {
                    Nodecl::NodeclBase lb1, lb2, ub1, ub2, stride1, stride2, stride;
                    if (lhs.is<Nodecl::Range>())
                    {
                        Nodecl::Range lhs_range = lhs.as<Nodecl::Range>();
                        lb1 = lhs_range.get_lower();
                        ub1 = lhs_range.get_upper();
                        stride1 = lhs_range.get_stride();
                    }
                    else
                    {
                        lb1 = lhs;
                        ub1 = lhs;
                        stride1 = Nodecl::NodeclBase::null();
                    }
                    if (rhs.is<Nodecl::Range>())
                    {
                        Nodecl::Range rhs_range = rhs.as<Nodecl::Range>();
                        lb2 = rhs_range.get_lower();
                        ub2 = rhs_range.get_upper();
                        stride2 = rhs_range.get_stride();
                    }
                    else
                    {
                        lb2 = rhs;
                        ub2 = rhs;
                        stride2 = Nodecl::NodeclBase::null();
                    }
                    
                    // Mix the strides
                    if (!stride1.is_null() && !stride2.is_null())
                    {
                        if (Nodecl::Utils::equal_nodecls(stride1, stride2))
                        {
                            stride = stride1;
                        }
                        else
                        {   // FIXME When both are a range and the renamed var is different, we have nested loops!
                            internal_error("Combination of induction variables coming from nested loops not yet implemented" \
                                            " when ranging induction variables", 0);
                        }
                    }
                    else if (!stride1.is_null())
                    {
                        stride = stride1;
                    }
                    else
                    {
                        stride = stride2;
                    }
                    
                    renamed = create_new_range_from_binary_node(n, lb1, ub1, lb2, ub2, stride);
                }
                else
                {
                    renamed = create_new_binary_node(n, lhs, rhs);
                }
                
                return ObjectList<Nodecl::NodeclBase>(1, renamed);            
            }
            
            return ObjectList<Nodecl::NodeclBase>();
        }

        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::Add& n)
        {
            return visit_binary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::Minus& n)
        {
            return visit_binary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::Mul& n)
        {
            return visit_binary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::Div& n)
        {
            return visit_binary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::Mod& n)
        {
            return visit_binary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::Power& n)
        {
            return visit_binary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::LogicalAnd& n)
        {
            return visit_binary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::LogicalOr& n)
        {
            return visit_binary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::BitwiseAnd& n)
        {
            return visit_binary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::BitwiseOr& n)
        {
            return visit_binary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::BitwiseXor& n)
        {
            return visit_binary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::Shr& n)
        {
            return visit_binary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::Shl& n)
        {
            return visit_binary(n);
        }
    
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::Assignment& n)
        {
            return visit_binary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::AddAssignment& n)
        {
            return visit_binary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::MinusAssignment& n)
        {
            return visit_binary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::DivAssignment& n)
        {
            return visit_binary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::MulAssignment& n)
        {
            return visit_binary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::ModAssignment& n)
        {
            return visit_binary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::BitwiseAndAssignment& n)
        {
            return visit_binary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::BitwiseOrAssignment& n)
        {
            return visit_binary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::BitwiseXorAssignment& n)
        {
            return visit_binary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::ShrAssignment& n)
        {
            return visit_binary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::ShlAssignment& n)
        {
            return visit_binary(n);
        }

        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::Equal& n)
        {
            return visit_binary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::Different& n)
        {
            return visit_binary(n);
        }
                
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::LowerThan& n)
        {
            return visit_binary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::GreaterThan& n)
        {
            return visit_binary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::LowerOrEqualThan& n)
        {
            return visit_binary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::GreaterOrEqualThan& n)
        {
            return visit_binary(n);
        }

        template <typename T>
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit_unary(const T& n)
        {
            Nodecl::NodeclBase rhs = n.get_rhs();
            ObjectList<Nodecl::NodeclBase> renamed_rhs = walk(rhs);
            
            if (!renamed_rhs.empty())
            {
                rhs = renamed_rhs[0];
                
                Nodecl::NodeclBase renamed;
                if (n.template is<Nodecl::Predecrement>())
                {
                    renamed = Nodecl::Predecrement::make(rhs, n.get_type(), _filename, _line);
                }
                else if (n.template is<Nodecl::Postdecrement>())
                {
                    renamed = Nodecl::Postdecrement::make(rhs, n.get_type(), _filename, _line);
                }
                else if (n.template is<Nodecl::Preincrement>())
                {
                    renamed = Nodecl::Preincrement::make(rhs, n.get_type(), _filename, _line);
                }
                else if (n.template is<Nodecl::Postincrement>())
                {           
                    renamed = Nodecl::Postincrement::make(rhs, n.get_type(), _filename, _line);
                }
                else if (n.template is<Nodecl::Plus>())
                {
                    renamed = Nodecl::Plus::make(rhs, n.get_type(), _filename, _line);
                }
                else if (n.template is<Nodecl::Neg>())
                {
                    renamed = Nodecl::Neg::make(rhs, n.get_type(), _filename, _line);
                }
                else if (n.template is<Nodecl::BitwiseNot>())
                {
                    renamed = Nodecl::BitwiseNot::make(rhs, n.get_type(), _filename, _line);
                }
                else if (n.template is<Nodecl::LogicalNot>())
                {
                    renamed = Nodecl::LogicalNot::make(rhs, n.get_type(), _filename, _line);
                }
                else if (n.template is<Nodecl::Derreference>())
                {
                    renamed = Nodecl::Derreference::make(rhs, n.get_type(), _filename, _line);
                }
                else if (n.template is<Nodecl::Reference>())
                {
                    renamed = Nodecl::Reference::make(rhs, n.get_type(), _filename, _line);
                }
                
                return ObjectList<Nodecl::NodeclBase>(1, renamed);
            }
            
            return ObjectList<Nodecl::NodeclBase>();
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::Predecrement& n)
        {
            return visit_unary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::Postdecrement& n)
        {
            return visit_unary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::Preincrement& n)
        {
            return visit_unary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::Postincrement& n)
        {
            return visit_unary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::Plus& n)
        {
            return visit_unary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::Neg& n)
        {
            return visit_unary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::BitwiseNot& n)
        {
            return visit_unary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::LogicalNot& n)
        {
            return visit_unary(n);
        }
    
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::Derreference& n)
        {
            return visit_unary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::Reference& n)
        {
            return visit_unary(n);
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::IntegerLiteral& n)
        {
            return ObjectList<Nodecl::NodeclBase>();
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::FloatingLiteral& n)
        {
            return ObjectList<Nodecl::NodeclBase>();
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::ComplexLiteral& n)
        {
            return ObjectList<Nodecl::NodeclBase>();
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::BooleanLiteral& n)
        {
            return ObjectList<Nodecl::NodeclBase>();
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::StringLiteral& n)
        {
            return ObjectList<Nodecl::NodeclBase>();
        }  
    
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::Conversion& n)
        {
            Nodecl::NodeclBase nest = n.get_nest();
            ObjectList<Nodecl::NodeclBase> renamed_nest = walk(nest);
            
            if (!renamed_nest.empty())
            {
                Nodecl::NodeclBase renamed = Nodecl::Conversion::make(renamed_nest[0], n.get_type(), _filename, _line);
                
                return ObjectList<Nodecl::NodeclBase>(1, renamed);
            }
            
            return ObjectList<Nodecl::NodeclBase>();
        }    
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::ConditionalExpression& n)
        {
            Nodecl::NodeclBase cond = n.get_condition();
            Nodecl::NodeclBase true_nodecl = n.get_true();
            Nodecl::NodeclBase false_nodecl = n.get_false();
            ObjectList<Nodecl::NodeclBase> renamed_cond = walk(cond);
            ObjectList<Nodecl::NodeclBase> renamed_true = walk(true_nodecl);
            ObjectList<Nodecl::NodeclBase> renamed_false = walk(false_nodecl);
            
            if (!renamed_cond.empty() || !renamed_true.empty() || !renamed_false.empty())
            {
                if (!renamed_cond.empty())
                {
                    cond = renamed_cond[0];
                }
                if (!renamed_true.empty())
                {
                    true_nodecl = renamed_true[0];
                }
                if (!renamed_false.empty())
                {
                    false_nodecl = renamed_false[0];
                }
                
                Nodecl::NodeclBase renamed = Nodecl::ConditionalExpression::make(cond, true_nodecl, false_nodecl, n.get_type(), _filename, _line);
                return ObjectList<Nodecl::NodeclBase>(1, renamed);
            }
        
            return ObjectList<Nodecl::NodeclBase>();
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::Cast& n)
        {
            Nodecl::NodeclBase rhs = n.get_rhs();
            ObjectList<Nodecl::NodeclBase> renamed_rhs = walk(rhs);
            
            if (!renamed_rhs.empty())
            {
                rhs =renamed_rhs[0];
                Nodecl::NodeclBase renamed = Nodecl::Cast::make(rhs, n.get_type(), n.get_text(), _filename, _line);
                return ObjectList<Nodecl::NodeclBase>(1, renamed);
            }
            
            return ObjectList<Nodecl::NodeclBase>();
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::FunctionCall& n)
        {
            Nodecl::NodeclBase called = n.get_called();
            Nodecl::NodeclBase arguments = n.get_arguments();
            ObjectList<Nodecl::NodeclBase> renamed_called = walk(called);
            ObjectList<Nodecl::NodeclBase> renamed_arguments = walk(arguments);
            
            if (!renamed_called.empty() || !renamed_arguments.empty())
            {
                if (!renamed_called.empty())
                {
                    called = renamed_called[0];
                }
                if (!renamed_arguments.empty())
                {
                    arguments = Nodecl::NodeclBase(create_nodecl_list(renamed_arguments));
                }

                Nodecl::NodeclBase renamed = Nodecl::VirtualFunctionCall::make(called, arguments, n.get_type(), _filename, _line);
                return ObjectList<Nodecl::NodeclBase>(1, renamed);
            }
        
            return ObjectList<Nodecl::NodeclBase>();
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::VirtualFunctionCall& n)
        {
            Nodecl::NodeclBase called = n.get_called();
            Nodecl::NodeclBase arguments = n.get_arguments();
            ObjectList<Nodecl::NodeclBase> renamed_called = walk(called);
            ObjectList<Nodecl::NodeclBase> renamed_arguments = walk(arguments);
            
            if (!renamed_called.empty() || !renamed_arguments.empty())
            {
                if (!renamed_called.empty())
                {
                    called = renamed_called[0];
                }
                if (!renamed_arguments.empty())
                {
                    arguments = Nodecl::NodeclBase(create_nodecl_list(renamed_arguments));
                }

                Nodecl::NodeclBase renamed = Nodecl::VirtualFunctionCall::make(called, arguments, n.get_type(), _filename, _line);
                return ObjectList<Nodecl::NodeclBase>(1, renamed);
            }
        
            return ObjectList<Nodecl::NodeclBase>();
        }
        
        CfgRenamingVisitor::Ret CfgRenamingVisitor::visit(const Nodecl::Sizeof& n)
        {   // No evaluation performed in the sizeof expression, so no renaming needed
            return ObjectList<Nodecl::NodeclBase>();
        }
    }
}
