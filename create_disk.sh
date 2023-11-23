#!/bin/bash
set -euxo pipefail
# Author: Vojtech Aschenbrenner <asch@cs.wisc.edu>

rm -f disk
truncate -s 1M disk
