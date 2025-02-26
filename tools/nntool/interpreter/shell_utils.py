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

import argparse
import logging
import os
import shutil
import weakref
from glob import glob
from itertools import zip_longest

import numpy as np
from cmd2 import Cmd, ansi

from graph.types import FilterParameters, ConvFusionParameters
from utils.data_importer import MODES
from utils.tabular import CSVRenderer, ExcelRenderer, TextTableRenderer
from utils.node_id import NodeId

class TableAction(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        if option_string in ("-x", "--xls"):
            setattr(namespace, self.dest, {"fmt": "xls", "filename": values})
        elif option_string in ("-c", "--csv"):
            setattr(namespace, self.dest, {"fmt": "csv", "filename": values})
        else:
            setattr(namespace, self.dest, {"fmt": "tab"})

def table_options(parser, default_width=120):
    parser_group = parser.add_mutually_exclusive_group()
    parser_group.add_argument('-t', '--terminal', action=TableAction, dest="output", nargs=0)
    parser_group.add_argument('-x', '--xls', action=TableAction,
                              metavar="FILE NAME", type=str, dest="output",
                              completer_method=Cmd.path_complete)
    parser_group.add_argument('-c', '--csv', action=TableAction,
                              metavar="FILE NAME", type=str, dest="output",
                              completer_method=Cmd.path_complete)
    parser.add_argument('-w', '--table_width',\
        type=int, help='maximum width of table if displayed to terminal', default=default_width)

def input_options(parser):
    parser.add_argument('input_files',
                        nargs="*",
                        help="test data file to input",
                        completer_method=Cmd.path_complete)
    parser.add_argument('-B', '--bit_shift',
                        type=float, default=None,
                        help="shift input before offset and divisor. Negative for right shift.")
    parser.add_argument('-D', '--divisor',
                        type=float, default=None,
                        help="divide all input data by this value")
    parser.add_argument('-O', '--offset',
                        type=float, default=None,
                        help="offset all input data by this value")
    parser.add_argument('-H', '--height',
                        type=int, default=None,
                        help="adjust image height to this value")
    parser.add_argument('-W', '--width',
                        type=int, default=None,
                        help="adjust image width this value")
    parser.add_argument('-T', '--transpose',
                        action="store_true", help='Swap W and H')
    parser.add_argument('-F', '--nptype',
                        choices=np.sctypeDict.keys(), default=None,
                        help='interpret pixels as this numpy type')
    parser.add_argument('-M', '--mode',
                        choices=MODES.keys(), default=None,
                        help="mode to import image in")
    parser.add_argument('-N', '--norm_func',
                        choices=MODES.keys(), default=None,
                        help="lambda function to apply on input in the form x: fn(x)")
    parser.add_argument('--rgb888_rgb565',
                        action="store_true",
                        help="convert 3 channel 8bits input into 1 channel 16bit rgb565")

def output_table(table, args):
    fmt = ('tab' if args.output is None else args.output['fmt'])
    if fmt == "xls":
        table.render(ExcelRenderer(args.output['filename']))
    elif fmt == "csv":
        with open(args.output['filename'], "w") as file:
            table.render(CSVRenderer(file))
    else:
        renderer = TextTableRenderer(args.table_width)
        table.render(renderer)
        print(renderer.get_output())

def filter_dirs(path: str) -> bool:
    return os.path.isdir(path)

def glob_input_files(input_files, graph_inputs=1):
    input_files_list = []
    for file in input_files:
        for globbed_file in glob(os.path.expanduser(file)):
            input_files_list.append(globbed_file)
    if len(input_files_list) % graph_inputs:
        raise ValueError("input files number is not divisible for graph inputs {}".format(graph_inputs))
    shard = int(len(input_files_list) / graph_inputs)
    return [[input_files_list[i+j] for i in range(0, len(input_files_list), shard)] \
                for j in range(shard)]

def find_choice(choices, val):
    hits = [p for p in choices if p.startswith(val)]
    if len(hits) == 1:
        return hits[0]
    raise ValueError("expecting one of {}".format(", ".join(choices)))

class NNToolShellLogHandler(logging.Handler):
    def __init__(self, shell: Cmd, logger, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._shell_ref = weakref.ref(shell)
        logger.addHandler(self)


    def emit(self, record: logging.LogRecord):
        shell = self._shell_ref()
        if not shell:
            return
        output = self.format(record)
        if record.levelno >= logging.ERROR:
            shell.poutput(ansi.style_error(output))
        elif record.levelno >= logging.WARN:
            shell.poutput(ansi.style_warning(output))
        else:
            shell.pfeedback(ansi.style_success(output))

def print_comparison(tensors):
    term_size = shutil.get_terminal_size(fallback=(80, 23))
    max_line_width = term_size.columns // 2 - 1
    printt = lambda x: np.array_str(x, max_line_width=max_line_width, precision=3, suppress_small=True).split('\n')
# pylint: disable=not-an-iterable
    out = [[printt(t) if t is not None else None for t in tensors[i]] for i in range(2)]
    max_len = 0
    for o1, o2 in zip(out[0], out[1]):
        if o1 is None:
            if o2 is not None:
                max_len = max(max_len, max(len(o) for o in o2))
        else:
            if o2 is None:
                max_len = max(max_len, max(len(o) for o in o1))
            else:
                max_len = max(max_len, max(len(o) for o in o1+o2))

    make_len = lambda a: a + " "*(max_len - len(a))
    combine = lambda a, b: a if b is None else " "*(max_len+1) + b if a is None\
        else make_len(a) + " " + b
    all_outs = [combine(l0, l1) for (o0, o1) in zip_longest(*out, fillvalue=[])\
        for (l0, l1) in zip_longest(o0 if o0 is not None else [], o1 if o1 is not None else [])]
    return all_outs

def print_tensors(tensors):
    max_line_width = shutil.get_terminal_size(fallback=(80, 23)).columns
    printt = lambda x: np.array_str(x, max_line_width=max_line_width)
    out = ["Step {}\n".format(step_idx) + printt(t) for step_idx, t in enumerate(tensors)]
    return out
