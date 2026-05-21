# SPDX-License-Identifier: BSD-2-Clause
# -*- coding: utf-8 -*-
#
#
# Copyright (c) 2021 MediaTek Inc.
#
import subprocess
import re
import os
import ast
import argparse


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--workdir',
        type=str,
        required=True,
        help='Working directory for generated files (absolute path)'
    )
    parser.add_argument(
        '--out',
        type=str,
        default='wifi_info.bzl',
        help='Output bzl file name'
    )
    return parser.parse_args()


def parse_config_opts_from_bzl(bzl_path):
    with open(bzl_path, "r") as f:
        content = f.read()

    match = re.search(
        r"config_opts\s*=\s*({.*?})\s*\n",
        content,
        re.DOTALL
    )
    if not match:
        return {}

    config_opts_str = match.group(1)
    chip_keys = re.findall(r'"([a-zA-Z0-9_]+)"\s*:', config_opts_str)
    chip_dict = {}

    for chip_key in chip_keys:
        chip_match = re.search(
            r'"%s"\s*:\s*({.*?}),' % chip_key,
            config_opts_str,
            re.DOTALL
        )
        if chip_match:
            chip_dict_str = chip_match.group(1)
            chip_dict_str = re.sub(
                r'([A-Za-z0-9_]+):',
                r'"\1":',
                chip_dict_str
            )
            chip_dict[chip_key] = ast.literal_eval(chip_dict_str)
    return chip_dict


def clean_define_val(val):
    v = val
    if v.startswith("'") and v.endswith("'") and len(v) > 2:
        v = v.replace("'", "")
    return v


def pylist_to_bzl(pylist):

    def quote_item(item):
        return '"' + str(item).replace('"', '\\"') + '"'

    return '[\n' + ''.join(
        '    {},\n'.format(quote_item(x)) for x in pylist
    ) + ']'


def main():

    args = parse_args()
    workdir = args.workdir
    out_bzl = args.out

    # 1. Parse config_opts in module_definition.bzl
    bzl_path = os.path.join(workdir, "module_definition.bzl")
    print("[DEBUG] bzl_path: {}".format(bzl_path))
    chip_config_opts = parse_config_opts_from_bzl(bzl_path)
    print("[DEBUG] Parsed chip list: {}".format(list(chip_config_opts.keys())))

    result_dict = {}

    if not chip_config_opts:
        print("[DEBUG] config_opts not found, output empty bzl variables")
        result_dict = {}
    else:
        for chip_key, opts in chip_config_opts.items():
            print("[DEBUG] Processing chip: {}".format(chip_key))
            env_vars = ["{}={}".format(k, v) for k, v in opts.items()]
            env_vars.append("DRIVER_DIR={}".format(args.workdir))
            print("[DEBUG] env_vars: {}".format(env_vars))
            env_str = " ".join(env_vars)
            makefile_temp_path = os.path.join(workdir, 'Makefile.temp')
            # cmd = f"{env_str} make -f {makefile_temp_path} -n print-ccflags"
            cmd = "{} make -f {} -n print-ccflags".format(
                env_str,
                makefile_temp_path
            )
            print("[DEBUG] Executing command: {}".format(cmd))

            try:
                result = subprocess.check_output(
                    cmd,
                    shell=True,
                    stderr=subprocess.STDOUT,
                    universal_newlines=True
                )
                print(
                    "[DEBUG] First 100 chars of output: {}".format(
                        result[:100]
                    )
                )
            except subprocess.CalledProcessError as e:
                result = e.output
                print("[DEBUG] error output: {}".format(result[:100]))

            local_defines = []
            include = []
            srcs = []
            define_seen = set()
            for m in re.finditer(r' -D([A-Za-z0-9_]+(=[^ \n]+)?)', result):
                define = m.group(1)
                key = define.split('=')[0]
                if (
                    key not in define_seen
                    and "KBUILD_BASENAME" not in key
                    and "KBUILD_MODNAME" not in key
                    and "__KERNEL__" not in key
                    and "MODULE" not in key
                    and "GENKSYMS" not in key
                ):
                    local_defines.append(define)
                    define_seen.add(key)
            for m in re.finditer(r' -I([^\s]+)', result):
                inc = m.group(1).rstrip('}]').rstrip('"').lstrip('"').replace("\\", "")
                if inc.startswith("./"):
                    continue
                inc = os.path.normpath(inc)
                if inc.startswith("DEVICE_MODULES_PATH"):
                    inc = inc.replace("DEVICE_MODULES_PATH", "$(DEVICE_MODULES_PATH)", 1)
                elif inc.startswith("srctree"):
                    inc = inc.replace("srctree", "$(srctree)", 1)
                elif inc.startswith("TOP"):
                    inc = inc.replace("TOP", "$(TOP)", 1)
                include.append(inc)
            for m in re.finditer(r'([A-Za-z0-9_/\.\-]+\.o)\b', result):
                src = m.group(1)
                src = src.replace('.o', '.c')
                basename = os.path.basename(src)
                if (
                    ".o.c" not in src
                    and basename != ".c"
                    and (basename.endswith(".c") or basename.endswith(".o"))
                ):
                    srcs.append(src)
            cleaned_defines = []
            for d in local_defines:
                if "=" in d:
                    k, v = d.split("=", 1)
                    v = clean_define_val(v)
                    cleaned_defines.append(f"{k}={v}")
                else:
                    cleaned_defines.append(d)
            local_defines = list(dict.fromkeys(cleaned_defines))
            # Convert path to relative to workdir
            include = [
                os.path.relpath(i, workdir) if i.startswith(workdir) else i
                for i in sorted(set(include))
            ]
            srcs = [
                os.path.relpath(s, workdir) if s.startswith(workdir) else s
                for s in sorted(set(srcs))
            ]
            print("[DEBUG] local_defines_{} num: {}".format(
                chip_key, len(local_defines)))
            print("[DEBUG] include_{} num: {}".format(
                chip_key, len(include)))
            print("[DEBUG] srcs_{} num: {}".format(
                chip_key, len(srcs)))
            result_dict[chip_key] = {
                "local_defines": local_defines,
                "include": include,
                "srcs": srcs
            }

    # 2. Output separate variables for each chip

    def dict_to_bzl(name, d):
        fout.write(f'{name} = {{\n')
        for chip_key, vals in d.items():
            fout.write(
                f'    "{chip_key}": {pylist_to_bzl(vals)},\n'
            )
        fout.write('}\n\n')

    with open(out_bzl, 'w', encoding='utf-8') as fout:
        if not result_dict:
            fout.write('local_defines = {}\n\n')
            fout.write('srcs = {}\n\n')
            fout.write('includes = {}\n\n')
        else:
            dict_to_bzl(
                'local_defines',
                {k: v["local_defines"] for k, v in result_dict.items()}
            )
            dict_to_bzl(
                'srcs',
                {k: v["srcs"] for k, v in result_dict.items()}
            )
            dict_to_bzl(
                'includes',
                {k: v["include"] for k, v in result_dict.items()}
            )
    print('[DEBUG] {} generated, unified dictionary variables'.format(out_bzl))


if __name__ == '__main__':

    main()
