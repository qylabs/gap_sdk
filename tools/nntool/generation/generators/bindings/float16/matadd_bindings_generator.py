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

from generation.bindings import (CommentBindingList, GNodeArgEdge,
                                 GNodeArgNode, NodeBindingList)
from generation.generator_decorators import QREC_FLOAT, QREC_MULT8, generation_function
from graph.types import (ActivationFusion, MatrixAddParameters,
                         PaddedAddFusionParameters)
from quantization.multiplicative.mulbias import set_add_in_scale
from utils.node_id import NodeId


@generation_function("bindings", (MatrixAddParameters, ActivationFusion, PaddedAddFusionParameters), qrec_types=(QREC_FLOAT,))
def matadd_bindings_generator(gen, node, qrec, in_eparams, out_eparams, cname) -> bool:
    step_idx = node.step_idx
    if isinstance(node, PaddedAddFusionParameters):
        cnodes = node.contained_nodes()
        add_node = [node for node in cnodes if isinstance(
            node, MatrixAddParameters)]
        if add_node:
            quants = [gen.G.quantization[NodeId(
                node, fnode)] for fnode in cnodes]
            set_matadd_bindings(gen, node, step_idx, in_eparams, out_eparams,
                                cname, quants[1], out_q=quants[-1])
            return True
        return False
    if isinstance(node, ActivationFusion):
        cnodes = node.contained_nodes()
        quants = [gen.G.quantization[NodeId(node, fnode)] for fnode in cnodes]
        if isinstance(cnodes[0], MatrixAddParameters):
            set_matadd_bindings(gen, cnodes[0], step_idx, in_eparams, out_eparams,
                                cname, quants[0], out_q=quants[1])
            return True
        return False
    set_matadd_bindings(gen, node, step_idx, in_eparams,
                        out_eparams, cname, qrec)
    return True


def set_matadd_bindings(gen, node, step_idx, in_eparams, out_eparams, cname, qrec, out_q=None):
    del step_idx
    if out_q is None:
        out_q = qrec
    set_add_in_scale(qrec)
    scaled_idx = qrec.cache['scaled_idx']
    not_scaled_idx = 0 if scaled_idx else 1
    gen.bindings.append(
        CommentBindingList("Node {} in1q {} in2q {} outq {}", cname,
                           qrec.in_qs[scaled_idx], qrec.in_qs[not_scaled_idx], out_q.out_qs[0])
    )
    gen.bindings.append(
        NodeBindingList(cname, GNodeArgEdge(in_eparams[scaled_idx]),
                        GNodeArgEdge(in_eparams[not_scaled_idx]),
                        GNodeArgEdge(out_eparams[0], "GNA_OUT")
                        ))
