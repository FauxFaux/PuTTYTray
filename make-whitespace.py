#!/usr/bin/env python3

import os
import shutil
import subprocess

script_dir = os.path.dirname(os.path.realpath(__file__))

tags = [
    "0.45",
    "0.46",
    "0.47",
    "0.48",
    "0.49",
    "0.50",
    "0.51",
    "0.52",
    "0.53",
    "0.54",
    "0.55",
    "0.56",
    "0.57",
    "0.58",
    "0.59",
    "0.60",
    "0.61",
    "0.62",
    "0.63",
    "0.64",
    "0.65",
    "0.66",
    "0.67",
    "0.68",
    "0.69",
    "0.70",
]


def git(x, dates=None):
    env = None
    if dates:
        env = dict(os.environ)
        env.update({
                'GIT_AUTHOR_DATE': dates[0],
                'GIT_COMMITTER_DATE': dates[1],
              })
    subprocess.check_call("git " + x, shell=True, env=env)


def dates(refid):
    cmd = 'git show --format="%ad|%cd" ' + refid + " | head -n1"
    ad, cd = subprocess.check_output(cmd, shell=True)\
        .decode('utf-8').strip().split('|')
    return (ad, cd)


def make_cleaning_branch_from(tag):
    merge_base = "$(git merge-base upstream/master {})".format(tag)
    git("checkout -B mws-clean " + merge_base)
    shutil.copy(script_dir + "/clang-format.yaml", ".clang-format")
    subprocess.check_call("find . \( -iname \*.h -o -iname \*.c " +
                          "-o -iname \*.cpp -o -iname \*.hpp \) -print0 " +
                          "| xargs -0 clang-format-6.0 -i",
                          shell=True)
    git("rm --ignore-unmatch -r 'doc/' CHECKLST.txt README")
    git("commit -am 'normalising ~{}'".format(tag), dates(merge_base))


start = tags.pop(0)
make_cleaning_branch_from(start)
git("checkout -B upclean")

for tag in tags:
    make_cleaning_branch_from(tag)
    git(("update-ref refs/heads/upclean $(" +
         "echo 'merge ~{} into upclean' | " +
         "git commit-tree 'HEAD^{{tree}}' -p upclean -p mws-clean)"
         ).format(tag), dates("mws-clean"))
