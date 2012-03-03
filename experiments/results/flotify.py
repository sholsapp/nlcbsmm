#!/usr/bin/env python

import os
import sys
import re
import json

DELIM = '|'

def filter_lines(lines):
  """
    Matches lines like:

      SEGV|1330657657253534|192.168.1.8|192.168.1.8|0xaa752000|2295

  """
  regex = re.compile('SEGV|\d+|\d+\.\d+\.\d+\.\d+i|\d+\.\d+\.\d+\.\d+|0x[a-zA-Z0-9]+|\d+')

  return [line for line in lines if re.match(regex, line)]


def pretty_print(data):
  """
  """

  for unique_user in set(d['faulter'] for d in data):
    sys.stdout.write('Data for %s\n' % unique_user)
    sys.stdout.write('[\n')
    for e in list((d['clock'], d['addr']) for d in [this for this in data if unique_user == this['faulter']]):
      sys.stdout.write('  [%s, %s],\n' % (e[0], e[1]))
    sys.stdout.write(']\n')


def translate(lines):
  """
  """
  data = list()
  for line in filter_lines(lines):
    # Parse fields
    try:
      msg_id, clock, faulter, faultee, addr, runtime = line.split(DELIM)
      # Add them to data
      data.append({
        'msg_id' : msg_id,
        'clock' : int(clock),
        'faulter' : faulter,
        'faultee' : faultee,
        'addr'    : int(addr, 16),
        'runtime' : int(runtime)
        });
    except Exception as e:
      print line

  time_base = min([d['clock'] for d in data])

  # Normalize times
  for d in data:
    d['clock'] = d['clock'] - time_base

  pretty_print(data)


def go():
  """
  """

  if len(sys.argv) < 2:
    sys.stdout.write("Usage: ./%s [inputs]\n" % sys.argv[0])

  cat_files = list()

  for a in sys.argv[1:]:
    with open(a, 'r') as fh:
      cat_files += fh.readlines()

  translate(cat_files)

if __name__ == '__main__':
  go()

