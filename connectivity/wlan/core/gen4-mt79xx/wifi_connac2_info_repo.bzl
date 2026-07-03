# wifi_connac2_info_repo.bzl
#
# This file defines a Bazel repository rule (wifi_connac2_info_repo) to automatically generate wifi_connac2_info.bzl
# by invoking an external Python script (gen_module_info.py). It uses the tool_files and additional_values attributes
# to generate module information and writes it to a .bzl file. A BUILD file is also generated for Skylib bzl_library management.
#
# Official documentation:
#   Bazel repository rules: https://bazel.build/docs/repo
#   Starlark repository_rule API: https://bazel.build/rules/lib/repository_rule
#
# Attributes:
#   tool_files: Must include gen_module_info.py and related dependencies (label_list, mandatory).
#   additional_values: Extra key-value pairs to append to the bzl file (string_dict, optional).
#
# Example usage (in MOUDLE.repo):
# wifi_connac2_info_repo(
#     name = "wifi_info_repo",
#     tool_files = ["//path/to:gen_module_info.py", ...],
#     additional_values = {"key": "value"},
# )
#
# This implementation runs gen_module_info.py in the repository sandbox, generates wifi_connac2_info.bzl,
# and creates a BUILD file for bzl_library. For details, refer to the official documentation above.

def _wifi_info_repo_impl(repository_ctx):
    #makefile_path = repository_ctx.path("Makefile")
    out_bzl = "wifi_connac2_info.bzl"

    # Get main_temp.py path (copied from tool_files to sandbox)
    print("tool_files list:")
    for f in repository_ctx.attr.tool_files:
        print("  tool_file: %s, sandbox path: %s" % (f, repository_ctx.path(f)))
    gen_module_info_py = None
    for f in repository_ctx.attr.tool_files:
        if repository_ctx.path(f).basename == "gen_module_info.py":
            gen_module_info_py = repository_ctx.path(f)
            print("Found gen_module_info.py path: %s" % gen_module_info_py)
            break
    if not gen_module_info_py:
        fail("gen_module_info.py not found, please declare gen_module_info.py in the tool_files parameter of WORKSPACE")

    # Call external python script to generate wifi_info.bzl
    # Get absolute path of sandbox working directory
    # repo_dir = str(repository_ctx.path("gen_module_info.py"))
    repo_dir = gen_module_info_py.dirname
    print("repo_dir path: %s" % repo_dir)
    print("out_bzl path: %s" % out_bzl)
    result = repository_ctx.execute([
        "python3",
        gen_module_info_py,
        "--workdir", repo_dir,
        "--out", out_bzl,
    ], timeout=21600)
    print("gen_module_info.py stdout:\n%s" % result.stdout)
    print("gen_module_info.py stderr:\n%s" % result.stderr)
    if result.return_code != 0:
        fail("gen_module_info.py parsing failed: %s" % result.stderr)

    bzl_content = repository_ctx.read(out_bzl)
    for key in repository_ctx.attr.additional_values:
        value = repository_ctx.attr.additional_values[key]
        bzl_content += '{} = "{}"\n'.format(key, value)

    repository_ctx.file(out_bzl, bzl_content, executable=False)

    repository_ctx.file("BUILD", """
load("@bazel_skylib//:bzl_library.bzl", "bzl_library")
bzl_library(
    name = "wifi_info_dict",
    srcs = ["wifi_info.bzl"],
    visibility = ["//visibility:public"],
)
""", executable=False)

wifi_connac2_info_repo = repository_rule(
    implementation = _wifi_info_repo_impl,
    local = True,
    attrs = {
        "tool_files": attr.label_list(allow_files = True, mandatory = True),
        "additional_values": attr.string_dict(default = {}),
    },
)
