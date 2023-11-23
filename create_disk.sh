#!/bin/bash
# Author: Vojtech Aschenbrenner <asch@cs.wisc.edu>

truncate -s 0M disk
set -euxo pipefail
truncate -s 1M disk
