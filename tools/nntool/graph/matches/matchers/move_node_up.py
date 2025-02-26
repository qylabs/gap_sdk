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

import inspect
import logging
from copy import deepcopy

from graph.types import (ActivationParameters, ConcatParameters,
                         Conv2DParameters, FcParameters, GlobalPoolingParameters,
                         MatrixAddParameters, MatrixMulParameters, NNEdge,
                         PoolingParameters, ReluActivationParameters,
                         ReshapeParameters, TransposeParameters)
from utils.graph import GraphView
from utils.node_id import NodeId

from ..matcher import Matcher, match_name, groups, run_before, description, needs_valid_dimension

LOG = logging.getLogger("nntool." + __name__)


class LocationNotFoundError(Exception):
    pass


class MoveNodeUpMatcher(Matcher):

    ValidNodesToPass = None
    ValidFusions = None
    ValidNodes = None

    def execute_tests(self, G, tests, node):
        if len(G.out_edges(node.name)) > 1:
            return False
        return any(isinstance(node, test) if inspect.isclass(test) else test(node) for test in tests)

    def find_home_for_node(self,
                           G,
                           node,
                           edge=None):
        if edge is None:
            in_edge = G.in_edges(node.name)[0]
            yield from self.find_home_for_node(G,
                                               node,
                                               edge=in_edge)
        elif self.execute_tests(G, self.ValidNodesToPass, edge.from_node):
            if isinstance(edge.from_node, ConcatParameters):
                for in_edge in G.in_edges(edge.from_node.name):
                    yield from self.find_home_for_node(G,
                                                       node,
                                                       edge=in_edge)
            else:
                in_edge = G.in_edges(edge.from_node.name)[0]
                yield from self.find_home_for_node(G,
                                                   node,
                                                   edge=in_edge)
        elif self.execute_tests(G, self.ValidFusions, edge.from_node):
            yield edge
        else:

            raise LocationNotFoundError()

    @staticmethod
    def move_node(G, node, edges):
        nid = NodeId(node)
        qrec = G.quantization[nid] if G.quantization and nid in G.quantization else None
        node_in_edge = G.in_edges(node.name)[0]
        node_out_edges = G.out_edges(node.name)
        G.remove(node)
        for node_out_edge in node_out_edges:
            new_edge = NNEdge(from_node=node_in_edge.from_node, to_node=node_out_edge.to_node,
                              from_idx=node_in_edge.from_idx, to_idx=node_out_edge.to_idx)
            G.add_edge(new_edge)
        cnt = 0
        original_node = node
        for edge in edges:
            LOG.info("Moving node %s between %s and %s",
                     node.name, edge.from_node.name, edge.to_node.name)
            if cnt > 0:
                new_node = deepcopy(node)
                new_node.name = f'{original_node.name}_{cnt}'
            else:
                new_node = node
            cnt += 1
            new_node.in_dims = [edge.from_node.out_dims[edge.from_idx].clone()]
            new_node.out_dims = [edge.to_node.in_dims[edge.to_idx].clone()]
            G.insert_node(new_node, edge.from_node, edge.to_node,
                          from_idx=edge.from_idx, to_idx=edge.to_idx,
                          edge_class=NNEdge)
            if qrec:
                from_qrec = G.quantization[NodeId(edge.from_node)]
                new_qrec = deepcopy(qrec)
                new_qrec.in_qs[0] = deepcopy(from_qrec.out_qs[edge.from_idx])
                G.quantization[NodeId(new_node)] = new_qrec
                G.quantization.propagate(
                    G, new_node, node_in_edge.from_node, qtype=new_qrec.out_qs[0])

    def _match(self, G: GraphView, set_identity: bool = True, **kwargs):
        target_nodes = [node for node in G.nodes(
        ) if self.execute_tests(G, self.ValidNodes, node)]
        target_nodes = filter(lambda n: not isinstance(
            G.in_edges(n.name)[0].from_node, self.ValidFusions), target_nodes)
        can_be_moved = []
        has_modified_graph = False
        for node in target_nodes:
            try:
                edges = list(self.find_home_for_node(G, node))
                LOG.info("Node %s can be moved", node.name)
                can_be_moved.append({'node': node, 'edges': edges})
            except LocationNotFoundError:
                LOG.info("Node %s cannot be moved", node.name)
        for move in can_be_moved:
            has_modified_graph = True
            self.move_node(G, move['node'], move['edges'])

        if set_identity:
            self.set_identity(G)

        return has_modified_graph


@groups('scaled')
@match_name("move_activations_scale8")
@description("Tries to move activations so they are after layers that they can be fused with."
             "Should be run before match_gap_ * fusions. Compatible with AutoTiler SQ8 kernels.")
@needs_valid_dimension(True)
@run_before('fuse_gap_convs', 'fuse_gap_linear', 'fuse_gap_pool', 'fuse_op_activation_scale8')
class MoveActivationsMatcherScale8(MoveNodeUpMatcher):

    ValidNodesToPass = (ReshapeParameters,
                        TransposeParameters, ConcatParameters)
    ValidFusions = (Conv2DParameters, FcParameters, PoolingParameters, PoolingParameters,
                    GlobalPoolingParameters, MatrixAddParameters, MatrixMulParameters)

    ValidNodes = (ActivationParameters,)


@groups('symmetric')
@match_name("move_activations_pow2")
@description("Tries to move activations so they are after layers that they can be fused with."
             "Should be run before match_gap_ * fusions. Compatible with AutoTiler POW2 kernels.")
@needs_valid_dimension(True)
@run_before('fuse_gap_convs', 'fuse_gap_linear', 'fuse_gap_pool', 'fuse_op_activation_pow2')
class MoveActivationsMatcherPow2(MoveNodeUpMatcher):

    ValidNodesToPass = (ReshapeParameters,
                        TransposeParameters, ConcatParameters)
    ValidFusions = (Conv2DParameters, FcParameters, PoolingParameters)
    ValidNodes = (ActivationParameters,)


@groups('scaled')
@match_name("move_pooling_scale8")
@description("Tries to move pooling layers so they are after layers that they can be fused with."
             "Should be run before match_gap_ * fusions. Compatible with AutoTiler SQ8 kernels.")
@needs_valid_dimension(True)
@run_before('fuse_gap_convs', 'fuse_gap_linear', 'fuse_gap_pool', 'fuse_op_activation_scale8')
class MoveMaxPoolMatcherScale8(MoveNodeUpMatcher):


    ValidNodesToPass = (ReshapeParameters, TransposeParameters,
                        ReluActivationParameters, ConcatParameters)
    ValidFusions = (Conv2DParameters, FcParameters)
    ValidNodes = (lambda node: isinstance(
        node, PoolingParameters) and node.pool_type == "max",)
