# Copyright (C) 2020  GreenWaves Technologies, SAS

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

from generation.at_types.tc_arg_info import LocalArgInfo
from generation.bindings import (CommentBindingList, GArgName, GNodeArgEdge,
                                 GNodeArgNode, NoArg, NodeBindingList)
from generation.generator_decorators import QREC_FLOAT, QREC_MULT8, generation_function
from generation.generators.globals.global_names import INFOS
from graph.types import (GRUParameters, LSTMParameters, RNNBaseParameters,
                         RNNParameters)


@generation_function("bindings", (RNNParameters, LSTMParameters, GRUParameters), qrec_types=(QREC_FLOAT,))
def rnn_bindings_generator(gen, node, qrec, in_eparams, out_eparams, cname) -> bool:
    step_idx = node.step_idx
    if isinstance(node, RNNParameters):
        set_rnn_bindings(gen, step_idx, in_eparams,
                         out_eparams, cname, node, qrec)
    elif isinstance(node, LSTMParameters):
        set_lstm_bindings(gen, step_idx, in_eparams,
                          out_eparams, cname, node, qrec)
    elif isinstance(node, GRUParameters):
        set_gru_bindings(gen, step_idx, in_eparams,
                         out_eparams, cname, node, qrec)

    return True


def num_sequences(node: RNNBaseParameters):
    n1 = min(node.n_input_cells, node.n_cells - node.n_output_cells)
    n3 = node.n_cells - max(node.n_input_cells,
                            node.n_cells - node.n_output_cells)
    n2 = node.n_cells - (n1 + n3)
    return sum(1 if n > 0 else 0 for n in [n1, n2, n3])


def set_lstm_bindings(gen, step_idx, in_eparams, out_eparams, cname,
                      rnn_params, rnn_q):

    names = rnn_params.get_name_indexes()

    gen.bindings.append(
        CommentBindingList("Node {} inq {} outq {}", cname,
                           rnn_q.in_qs[0],
                           rnn_q.out_qs[0])
    )
    c_state_eparams = in_eparams[names['c_state']]
    i_state_eparams = in_eparams[names['i_state']]

    num_seq = num_sequences(rnn_params)
    if num_seq > 1:
        gen.locals.append(LocalArgInfo(
            "int8", "S%s_CellInternal01" % step_idx))
        gen.locals.append(LocalArgInfo(
            "int8", "S%s_StateInternal01" % step_idx))

    if num_seq > 2:
        gen.locals.append(LocalArgInfo(
            "int8", "S%s_CellInternal02" % step_idx))
        gen.locals.append(LocalArgInfo(
            "int8", "S%s_StateInternal02" % step_idx))

    reset_name = i_state_eparams.creating_node.reset_name if not rnn_params.rnn_states_as_inputs else f"{rnn_params.name}_Reset"
    gen.bindings.append(
        NodeBindingList(cname,
                        GNodeArgEdge(c_state_eparams, direction="GNA_INOUT"),
                        GNodeArgEdge(i_state_eparams, direction="GNA_INOUT"),
                        GNodeArgEdge("S%s_CellInternal01" % step_idx, alias=c_state_eparams,
                                     direction="GNA_INOUT") if num_seq > 1 else NoArg(),
                        GNodeArgEdge("S%s_StateInternal01" % step_idx, alias=i_state_eparams,
                                     direction="GNA_INOUT") if num_seq > 1 else NoArg(),
                        GNodeArgEdge("S%s_CellInternal02" % step_idx, alias="S%s_CellInternal01" %
                                     step_idx, direction="GNA_INOUT") if num_seq > 2 else NoArg(),
                        GNodeArgEdge("S%s_StateInternal02" % step_idx, alias="S%s_CellInternal01" %
                                     step_idx, direction="GNA_INOUT") if num_seq > 2 else NoArg(),
                        GNodeArgEdge(in_eparams[0]),
                        GNodeArgEdge(in_eparams[names['r_2_f_w']]),
                        GNodeArgEdge(in_eparams[names['f_b']]),
                        GNodeArgEdge(in_eparams[names['r_2_i_w']]),
                        GNodeArgEdge(in_eparams[names['i_b']]),
                        GNodeArgEdge(in_eparams[names['r_2_c_w']]),
                        GNodeArgEdge(in_eparams[names['c_b']]),
                        GNodeArgEdge(in_eparams[names['r_2_o_w']]),
                        GNodeArgEdge(in_eparams[names['o_b']]),
                        GNodeArgEdge(out_eparams[0], direction="GNA_OUT"),
                        GArgName(reset_name)
                        ))


def set_rnn_bindings(gen, step_idx, in_eparams, out_eparams, cname,
                     rnn_params, rnn_q):
    names = rnn_params.get_name_indexes()

    gen.bindings.append(
        CommentBindingList("Node {} inq {} weightsq {} outq {} biasesq {}", cname,
                           rnn_q.in_qs[0], rnn_q.in_qs[names['r_2_i_w']],
                           rnn_q.out_qs[0], rnn_q.in_qs[names['i_b']])
    )
    num_seq = num_sequences(rnn_params)
    if num_seq > 1:
        gen.locals.append(LocalArgInfo(
            "int8", "S%s_StateInternal01" % step_idx))

    if num_seq > 2:
        gen.locals.append(LocalArgInfo(
            "int8", "S%s_StateInternal02" % step_idx))

    i_state_eparams = in_eparams[names['i_state']]
    reset_name = i_state_eparams.creating_node.reset_name if not rnn_params.rnn_states_as_inputs else "Reset"
    gen.bindings.append(
        NodeBindingList(cname,
                        GNodeArgEdge(i_state_eparams, direction="GNA_INOUT"),
                        GNodeArgEdge("S%s_StateInternal01" % step_idx, alias=i_state_eparams,
                                     direction="GNA_INOUT") if num_seq > 1 else NoArg(),
                        GNodeArgEdge("S%s_StateInternal02" % step_idx, alias="S%s_CellInternal01" %
                                     step_idx, direction="GNA_INOUT") if num_seq > 2 else NoArg(),
                        GNodeArgEdge(in_eparams[0]),
                        GNodeArgEdge(in_eparams[names['r_2_i_w']]),
                        GNodeArgEdge(in_eparams[names['i_b']]),
                        GNodeArgEdge(out_eparams[0], direction="GNA_OUT"),
                        GArgName(reset_name)
                        ))


def set_gru_bindings(gen, step_idx, in_eparams, out_eparams, cname,
                     rnn_params, rnn_q):
    names = rnn_params.get_name_indexes()

    gen.bindings.append(
        CommentBindingList("Node {} inq {} outq {}", cname,
                           rnn_q.in_qs[0],
                           rnn_q.out_qs[0])
    )
    num_seq = num_sequences(rnn_params)
    if num_seq > 1:
        gen.locals.append(LocalArgInfo(
            "int8", "S%s_StateInternal01" % step_idx))

    if num_seq > 2:
        gen.locals.append(LocalArgInfo(
            "int8", "S%s_StateInternal02" % step_idx))

    i_state_eparams = in_eparams[names['h_state']]
    reset_name = i_state_eparams.creating_node.reset_name if not rnn_params.rnn_states_as_inputs else "Reset"
    gen.bindings.append(
        NodeBindingList(cname,
                        GNodeArgEdge(i_state_eparams, direction="GNA_INOUT"),
                        GNodeArgEdge("S%s_StateInternal01" % step_idx, alias=i_state_eparams,
                                     direction="GNA_INOUT") if num_seq > 1 else NoArg(),
                        GNodeArgEdge("S%s_StateInternal02" % step_idx, alias="S%s_CellInternal01" %
                                     step_idx, direction="GNA_INOUT") if num_seq > 2 else NoArg(),
                        GNodeArgEdge(in_eparams[0]),
                        GNodeArgEdge(in_eparams[names['r_2_r_w']]),
                        GNodeArgEdge(in_eparams[names['r_b']]),
                        GNodeArgEdge(in_eparams[names['r_2_z_w']]),
                        GNodeArgEdge(in_eparams[names['z_b']]),
                        GNodeArgEdge(in_eparams[names['r_2_h_w']]),
                        GNodeArgEdge(in_eparams[names['w_h_b']]),
                        GNodeArgEdge(in_eparams[names['r_h_b']]),
                        GNodeArgEdge(out_eparams[0], direction="GNA_OUT"),
                        GArgName(reset_name)
                        ))
